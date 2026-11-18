#include "src/storyland/storyland_player_restart.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_play_test_script.h"
#include "src/storyland/storyland_mission_radar.h"
#include "src/storyland/storyland_service_population.h"
#include "src/storyland/storyland_vehicle_entry.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include "src/leeds/leeds_scm_definitions.inc"

namespace {
bool finiteRestart(const StorylandMissionEnvironment::PoliceRestart& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z) && std::isfinite(point.heading);
}

StorylandPlayerRestartPoint restartPoint(const StorylandMissionEnvironment::PoliceRestart& point,
    bool overridden) {
    return {{point.x,point.y,point.z},point.heading + 90.0f,overridden};
}

uint32_t restartMapLevelAt(const StorylandNativeZones& zones,
    const StorylandNativePoint& position,uint32_t currentLevel) {
    // VCS 0x367880 uses the 3D MAP table, not navigation zones. Unlike
    // the rendering lookup, an unmatched position returns generic level zero.
    if(position.z <= -100.0f) return 3;
    for(size_t index = 1; index < zones.map.size(); ++index) {
        const auto& zone = zones.map[index];
        if(zone.contains(position) && (zone.level == currentLevel || (zone.level >= 1 && zone.level <= 3)))
            return zones.worldLevelAt(position,currentLevel);
    }
    return 0;
}

struct RestartScmOperand {
    int32_t bits = 0;
    bool immediate = false;
};

bool readRestartScmOperand(const std::vector<uint8_t>& scm,size_t& cursor,size_t end,
    bool vcs,RestartScmOperand& operand) {
    operand = {};
    if(cursor >= end) return false;
    const uint8_t kind = scm[cursor++];
    if(kind == 0) return false;
    if(kind == 10) {
        // The native text operand has a type byte. On LCS its text is limited
        // to eight characters, while VCS uses a zero-terminated string.
        const unsigned maximum=vcs?128u:8u;
        unsigned consumed=0;
        while(consumed<maximum && cursor<end) {
            ++consumed;
            if(scm[cursor++]==0)return true;
        }
        return !vcs&&consumed==maximum;
    }
    size_t count = 0;
    if(kind <= 9) {
        operand.immediate = true;
        if(kind >= 3 && kind <= 5) count = kind - 2;
        else if(kind == 6 || kind == 9) count = 4;
        else if(kind == 7) count = 1;
        else if(kind == 8) count = 2;
        if(end - cursor < count) return false;
        uint32_t bits = 0;
        for(size_t index = 0; index < count; ++index)
            bits |= uint32_t(scm[cursor++]) << (index * 8);
        if(kind >= 3 && kind <= 5) bits <<= (4 - count) * 8;
        operand.bits = kind == 7 ? int8_t(bits) : kind == 8 ? int16_t(bits) : int32_t(bits);
        return true;
    }
    const unsigned local = vcs ? 13 : 12;
    const unsigned localArray = vcs ? 109 : 108;
    const unsigned global = vcs ? 205 : 204;
    if(kind < local) return kind >= (vcs ? 11 : 10);
    count = kind >= 230 ? 3 : kind >= global ? 1 : kind >= localArray ? 2 : 0;
    if(end - cursor < count) return false;
    cursor += count;
    return true;
}

bool restartScmTextArgument(const char* description,unsigned argument) {
    for(const char* cursor = description; *cursor; ++cursor) {
        if(*cursor != '%') continue;
        unsigned number = 0;
        const char* position = cursor + 1;
        while(*position >= '0' && *position <= '9')
            number = number * 10 + unsigned(*position++ - '0');
        if(number == argument && (*position == 's' || *position == 'g')) return true;
    }
    return false;
}

bool restartInitializerRange(const std::vector<uint8_t>& scm,int32_t mission,
    uint32_t mainOffset,uint32_t mainEnd,bool vcs,size_t& begin,size_t& end) {
    if(mission < 0) return false;
    const auto word = [&](size_t offset) {
        return uint32_t(scm[offset]) | (uint32_t(scm[offset+1]) << 8) |
            (uint32_t(scm[offset+2]) << 16) | (uint32_t(scm[offset+3]) << 24);
    };
    const auto headerJump = [&](size_t offset,size_t& target) {
        if(offset > scm.size() || scm.size() - offset < 7 || scm[offset] != 2 || scm[offset+1] != 0 || scm[offset+2] != 6)
            return false;
        const uint64_t position = uint64_t(word(offset + 3)) + 8;
        if(position > mainOffset) return false;
        target = size_t(position);
        return true;
    };
    size_t second = 0,third = 0,entry = 0;
    if(!headerJump(8,second) || !headerJump(second,third) || !headerJump(third,entry) ||
        entry != mainOffset || third > mainOffset || mainOffset - third < 20) return false;
    const size_t count = size_t(scm[third+16]) | (size_t(scm[third+17]) << 8);
    const size_t table = third + 20;
    if(size_t(mission) >= count || count > (mainOffset - table) / 4) return false;
    const uint64_t selected = uint64_t(word(table + size_t(mission) * 4)) + 8;
    if(selected < mainEnd || selected >= scm.size()) return false;
    begin = size_t(selected);
    end = scm.size();
    for(size_t index = 0; index < count; ++index) {
        const uint64_t candidate = uint64_t(word(table + index * 4)) + 8;
        if(candidate < mainEnd || candidate > scm.size()) return false;
        if(candidate > begin && candidate < end) end = size_t(candidate);
    }
    size_t cursor = begin;
    for(unsigned instruction = 0; instruction < 4 && cursor < end && end - cursor >= 2; ++instruction) {
        const uint16_t opcode = uint16_t(scm[cursor]) | (uint16_t(scm[cursor+1]) << 8);
        cursor += 2;
        if(opcode == 0 || opcode == (vcs ? 0x01EA : 0x031C)) continue;
        if(opcode != (vcs ? 0x0238 : 0x03A9)) return false;
        if(vcs) {
            if(cursor >= end || scm[cursor++] != 10) return false;
        } else if(end - cursor < 8) return false;
        return end - cursor >= 4 && scm[cursor] == 'I' && scm[cursor+1] == 'N' &&
            scm[cursor+2] == 'I' && scm[cursor+3] == 'T';
    }
    return false;
}
}

static bool loadPlayerRestartDefinitionsWithProfile(StorylandMissionEnvironment& world,
    const std::vector<uint8_t>& scm,uint32_t mainOffset,uint32_t mainEnd,bool vcs,
    bool typedVcs03f0,bool& visitedVcs03f0,std::string& error) {
    error.clear();
    if(mainOffset >= mainEnd || mainEnd > scm.size()) {
        error = "Native MAIN restart bootstrap has invalid script bounds.";
        return false;
    }
    std::map<uint16_t,const StorylandScmDefinition*> definitions;
    if(vcs) for(const auto& definition : vcsDefinitions) definitions.emplace(definition.opcode,&definition);
    else for(const auto& definition : lcsDefinitions) definitions.emplace(definition.opcode,&definition);
    struct Definition { uint16_t opcode = 0; StorylandMissionEnvironment::PoliceRestart point; };
    std::map<size_t,Definition> restarts;
    struct Location { size_t offset = 0,base = 0,begin = 0,end = 0; };
    std::vector<Location> pending{{mainOffset,mainOffset,mainOffset,mainEnd}};
    std::set<std::pair<size_t,size_t>> visited;
    const uint16_t hospital = vcs ? 0x00CB : 0x0171;
    const uint16_t police = vcs ? 0x00CC : 0x0172;
    while(!pending.empty()) {
        const Location location = pending.back();
        const size_t offset = location.offset,scriptEnd = location.end;
        pending.pop_back();
        if(offset < location.begin || offset >= scriptEnd || visited.count({offset,location.base})) continue;
        if(visited.size() >= 100000) {
            error = "Native MAIN restart bootstrap exceeded its control-flow budget.";
            return false;
        }
        visited.insert({offset,location.base});
        auto fail = [&](const char* reason) {
            std::ostringstream message;
            message << "Native MAIN restart bootstrap " << reason << " at 0x" << std::hex << offset << '.';
            error = message.str();
            return false;
        };
        if(scriptEnd - offset < 2) return fail("has a truncated opcode");
        const uint16_t opcode = (uint16_t(scm[offset]) | (uint16_t(scm[offset+1]) << 8)) & 0x7FFF;
        const auto found = definitions.find(opcode);
        if(found == definitions.end()) {
            std::ostringstream message;
            message << "Native MAIN restart parser cannot decode opcode 0x" << std::hex << opcode
                << " at byte 0x" << offset << ".";
            error=message.str();
            return false;
        }
        if(vcs && opcode == 0x03F0) visitedVcs03f0=true;
        const auto& specification = *found->second;
        size_t cursor = offset + 2;
        std::vector<RestartScmOperand> arguments;
        const bool function = opcode == (vcs ? 0x037A : 0x05AE) || opcode == (vcs ? 0x037B : 0x05AF);
        if(function) {
            if(scriptEnd - cursor < 3) return fail("has a truncated function header");
            const unsigned inputs = scm[cursor++], outputs = scm[cursor++], localBase = scm[cursor++];
            if(inputs > 32 || outputs > 32 || localBase + inputs + outputs > 128)
                return fail("has an invalid function header");
            for(unsigned index = 0; index < inputs + outputs + 1; ++index) {
                RestartScmOperand value;
                if(!readRestartScmOperand(scm,cursor,scriptEnd,vcs,value)) return fail("has a truncated function argument");
                arguments.push_back(value);
            }
        } else if(specification.parameters < 0) {
            RestartScmOperand target;
            if(!readRestartScmOperand(scm,cursor,scriptEnd,vcs,target)) return fail("has a truncated thread target");
            arguments.push_back(target);
            unsigned count = 0;
            while(cursor < scriptEnd && scm[cursor] != 0) {
                RestartScmOperand value;
                if(++count > 128 || !readRestartScmOperand(scm,cursor,scriptEnd,vcs,value))
                    return fail("has invalid variable arguments");
            }
            if(cursor >= scriptEnd) return fail("has no variable-argument terminator");
            ++cursor;
        } else {
            if(specification.parameters > 64) return fail("has an invalid argument count");
            for(int index = 0; index < specification.parameters; ++index) {
                RestartScmOperand value;
                if(!vcs && restartScmTextArgument(specification.text,unsigned(index + 1))) {
                    if(cursor >= scriptEnd)return fail("has a truncated text argument");
                    if(scm[cursor] == 10) {
                        if(!readRestartScmOperand(scm,cursor,scriptEnd,vcs,value))
                            return fail("has a truncated typed text argument");
                    } else {
                        if(scriptEnd - cursor < 8)return fail("has a truncated inline text argument");
                        cursor+=8;
                    }
                } else if(!readRestartScmOperand(scm,cursor,scriptEnd,vcs,value))
                    return fail("has a truncated argument");
                arguments.push_back(value);
            }
        }
        if(vcs && opcode == 0x03F0 && typedVcs03f0) {
            // The examined PS2 handler consumes no operands. Some supplied
            // MAIN scripts nevertheless encode a typed operand after 03F0.
            // Decode that layout only if the native zero-operand scan fails;
            // do not change the interpreter's original opcode definition.
            RestartScmOperand unused;
            if(!readRestartScmOperand(scm,cursor,scriptEnd,true,unused))
                return fail("has a truncated 03F0 compatibility operand");
        }
        if(opcode == hospital || opcode == police || (vcs && opcode == 0x0363)) {
            if(arguments.size() != 4) return fail("has an invalid restart argument count");
            std::array<float,4> values{};
            for(size_t index = 0; index < values.size(); ++index) {
                if(!arguments[index].immediate) return fail("requires runtime values for a restart definition");
                std::memcpy(&values[index],&arguments[index].bits,sizeof(float));
                if(!std::isfinite(values[index])) return fail("has non-finite restart coordinates");
            }
            if(opcode != 0x0363 && !storylandNormalizeRestartHeight(world.groundHeightAt,
                values[0],values[1],values[2],vcs,error)) return false;
            restarts.emplace(offset,Definition{opcode,{values[0],values[1],values[2],values[3]}});
        }
        const bool branch = opcode == 0x0002 || opcode == (vcs ? 0x0021 : 0x004C) ||
            opcode == (vcs ? 0x0022 : 0x004D) || opcode == (vcs ? 0x0024 : 0x004F) ||
            opcode == (vcs ? 0x0079 : 0x00DC) || opcode == (vcs ? 0x0025 : 0x0050) ||
            opcode == (vcs ? 0x01BA : 0x02D2) || function;
        if(branch) {
            if(arguments.empty() || !arguments.front().immediate) return fail("has a dynamic branch target");
            const int32_t label = arguments.front().bits;
            const int64_t target = label < 0 ? int64_t(location.base) - label : int64_t(label) + 8;
            if(target >= int64_t(location.begin) && target < int64_t(scriptEnd))
                pending.push_back({size_t(target),location.base,location.begin,scriptEnd});
            else if(target >= mainOffset && target < mainEnd)
                pending.push_back({size_t(target),location.base,mainOffset,mainEnd});
            else return fail("branches outside its native script range");
        }
        if(opcode == (vcs ? 0x0289 : 0x041C) && !arguments.empty() && arguments.front().immediate) {
            size_t begin = 0,end = 0;
            if(restartInitializerRange(scm,arguments.front().bits,mainOffset,mainEnd,vcs,begin,end))
                pending.push_back({begin,begin,begin,end});
        }
        const bool returns = opcode == (vcs ? 0x0026 : 0x0051) ||
            opcode == (vcs ? 0x005E : 0x00C5) || opcode == (vcs ? 0x005F : 0x00C6);
        if(opcode != 0x0002 && opcode != (vcs ? 0x0023 : 0x004E) && !returns)
            pending.push_back({cursor,location.base,location.begin,scriptEnd});
    }
    std::vector<StorylandMissionEnvironment::PoliceRestart> hospitals,policeStations,taxiStarts;
    for(const auto& definition : restarts) {
        auto& points = definition.second.opcode == hospital ? hospitals :
            definition.second.opcode == police ? policeStations : taxiStarts;
        points.push_back(definition.second.point);
    }
    const size_t capacity = 8;
    if(hospitals.empty() || policeStations.empty() || hospitals.size() > capacity || policeStations.size() > capacity || taxiStarts.size() > 16) {
        error = "Native MAIN does not define valid hospital and police restart pools.";
        return false;
    }
    if(world.hospitalRestarts.empty()) world.hospitalRestarts = std::move(hospitals);
    if(world.policeRestarts.empty()) world.policeRestarts = std::move(policeStations);
    if(vcs && world.taxiRestarts.empty()) world.taxiRestarts = std::move(taxiStarts);
    return true;
}

bool storylandLoadPlayerRestartDefinitions(StorylandMissionEnvironment& world,
    const std::vector<uint8_t>& scm,uint32_t mainOffset,uint32_t mainEnd,bool vcs,
    std::string& error) {
    bool visitedVcs03f0=false;
    if(loadPlayerRestartDefinitionsWithProfile(world,scm,mainOffset,mainEnd,vcs,
        false,visitedVcs03f0,error))return true;
    if(!visitedVcs03f0)return false;
    const std::string nativeError=error;
    if(loadPlayerRestartDefinitionsWithProfile(world,scm,mainOffset,mainEnd,vcs,
        true,visitedVcs03f0,error))return true;
    error=nativeError;
    return false;
}

bool storylandBeginPlayerRestart(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandPlayerRestartState& state,
    StorylandPlayerRestartCause cause) {
    const bool intro = world.introMissionRestartQueued || (world.introMissionProtected &&
        cause != StorylandPlayerRestartCause::None);
    if((world.cutscene.playing && !intro) || !storylandBeginPlayerRestart(state,cause,
        world.gameMilliseconds,cause == StorylandPlayerRestartCause::Arrest ?
            std::max(runtime.arrestWanted,runtime.wanted) : runtime.wanted)) return false;
    state.introMissionRestart = intro;
    if(intro) world.introMissionRestartQueued = true;
    if(cause == StorylandPlayerRestartCause::Death)
        storylandBeginPlayerDeath(runtime,storylandNativeDeathAssociation(runtime.crime.vcs,0,-1));
    runtime.wastedActive = true;
    if(cause != StorylandPlayerRestartCause::Death)runtime.playerDeathTime = 0;
    runtime.respawnFade = 0;
    runtime.playerControl = false;
    storylandClearRestartTaxi(runtime,world,runtime.restartTaxi);
    runtime.playerMelee = {};
    runtime.playerReaction.reset();
    runtime.playerBulletReaction={};
    runtime.playerVehicleContact={};
    world.playerWalkTask.clear();
    world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
    runtime.vehicleExitPose = {};
    runtime.playerCrouchPose = {};
    runtime.debugMovement = StorylandDebugMovement::Walking;
    storylandClearPlayTestTarget(runtime);
    storylandClearAdrenaline(runtime);
    storylandCancelKillFrenzy(runtime);
    world.control = false;
    if(cause == StorylandPlayerRestartCause::Death) {
        runtime.statistics["TimesDied"] += 1;
        world.statistics["TimesDied"] = runtime.statistics["TimesDied"];
    } else if(cause == StorylandPlayerRestartCause::Arrest) {
        runtime.statistics["TimesArrested"] += 1;
        world.statistics["TimesArrested"] = runtime.statistics["TimesArrested"];
        world.playerArrested = true;
    }
    return true;
}

bool storylandChoosePlayerRestart(StorylandMissionEnvironment& world,
    const StorylandNativePoint& origin,StorylandPlayerRestartCause cause,bool vcs,
    StorylandPlayerRestartPoint& point,std::string& error) {
    if(world.introMissionProtected || world.introMissionRestartQueued) {
        error = "The intro mission must restart instead of using hospital or police recovery.";
        return false;
    }
    error.clear();
    if(!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)) {
        error = "Player restart origin is not finite.";
        return false;
    }
    if(world.nextRestartOverrideActive) {
        if(!finiteRestart(world.nextRestartOverride)) {
            error = "Mission restart override is not finite.";
            return false;
        }
        point = restartPoint(world.nextRestartOverride,true);
        world.nextRestartOverrideActive = false;
        return true;
    }
    if(world.restartOverrideEnabled) {
        if(!finiteRestart(world.restartOverride)) {
            error = "Native restart override is not finite.";
            return false;
        }
        point = restartPoint(world.restartOverride,true);
        world.restartOverrideEnabled = false;
        return true;
    }
    const bool police = cause != StorylandPlayerRestartCause::Death;
    if(!vcs) {
        StorylandMissionEnvironment::PoliceRestart selected;
        if(storylandChooseLcsRestart(world,{origin.x,origin.y,origin.z},police,selected)) {
            point = restartPoint(selected,false);
            return true;
        }
        error = "Native LCS restart points and pedestrian path fallback are unavailable.";
        return false;
    }
    const auto& points = police ? world.policeRestarts : world.hospitalRestarts;
    const uint32_t level = world.zones ? restartMapLevelAt(*world.zones,origin,world.currentWorldLevel) : 0;
    const StorylandMissionEnvironment::PoliceRestart* selected = nullptr;
    double nearest = 4000.0 * 4000.0;
    // VCS 0x371094 includes all levels when the current level is generic.
    // Unlike LCS' <= selection, VCS uses a strict distance comparison.
    for(unsigned pass = 0; pass < 2 && !selected; ++pass) {
        for(const auto& candidate : points) {
            if(!finiteRestart(candidate)) continue;
            if(pass == 0 && level != 0 && world.zones &&
                restartMapLevelAt(*world.zones,{candidate.x,candidate.y,candidate.z},world.currentWorldLevel) != level) continue;
            const double dx = double(candidate.x) - origin.x;
            const double dy = double(candidate.y) - origin.y;
            const double dz = double(candidate.z) - origin.z;
            const double distance = dx * dx + dy * dy + dz * dz;
            if(distance < nearest) {
                nearest = distance;
                selected = &candidate;
            }
        }
    }
    if(selected) {
        point = restartPoint(*selected,false);
        return true;
    }
    if(world.pedestrianPaths && world.pedestrianPaths->vcs) {
        const auto& nodes = world.pedestrianPaths->nodes;
        const StorylandNativePathNode* selectedNode = nodes.empty() ? nullptr : &nodes.front();
        float distanceLimit = 999999.9f;
        for(const auto& node : nodes) {
            if(node.vehicle || node.water || !std::isfinite(node.position.x) ||
                !std::isfinite(node.position.y) || !std::isfinite(node.position.z)) continue;
            const float distance = std::fabs(node.position.x - origin.x) +
                std::fabs(node.position.y - origin.y) + 3 * std::fabs(node.position.z - origin.z);
            if(distance < distanceLimit) {
                distanceLimit = distance;
                selectedNode = &node;
            }
        }
        if(selectedNode && std::isfinite(selectedNode->position.x) &&
            std::isfinite(selectedNode->position.y) && std::isfinite(selectedNode->position.z)) {
            point = {selectedNode->position,90.0f,false};
            return true;
        }
    }
    error = "Native VCS restart points and pedestrian path fallback are unavailable.";
    return false;
}

namespace {
void storylandResetPlayerRestartBody(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,const StorylandPlayerRestartPoint& point,bool vcs) {
    if(runtime.occupiedVehicle >= 0 && size_t(runtime.occupiedVehicle) < runtime.vehicles.size() &&
        runtime.playerVehicleSeat < 0) {
        auto& vehicle = runtime.vehicles[size_t(runtime.occupiedVehicle)];
        vehicle.hasDriver = false;
        vehicle.missionDriver = false;
        vehicle.parked = true;
        vehicle.aiActive = false;
        vehicle.speed = vehicle.lateralSpeed = vehicle.throttle = 0;
    }
    runtime.occupiedVehicle = runtime.transitionVehicle = -1;
    runtime.playerVehicleSeat = -1;
    runtime.vehicleAction = StorylandVehicleAction::None;
    runtime.vehiclePhase = {};
    runtime.vehicleTransitionPrepared = false;
    runtime.vehicleApproach.clear();
    runtime.vehicleActionTime = runtime.vehicleActionDuration = 0;
    runtime.playerWater = {};
    runtime.playerMelee = {};
    runtime.playerReaction.reset();
    runtime.playerDeath = {};
    runtime.playerCorpseBlood = {};
    runtime.playerBulletReaction={};
    runtime.playerVehicleContact={};
    world.playerWalkTask.clear();
    world.playerVehicleEntryTarget=-1;world.playerVehicleEntryAsPassenger=false;
    runtime.vehicleExitPose = {};
    runtime.playerCrouchPose = {};
    runtime.manualAimActive = false;
    runtime.playerAiming = false;
    runtime.playerStopVariant = -1;
    runtime.playerStopTime = runtime.playerMoveBlend = runtime.playerSprintBlend = 0;
    storylandResetPlayerSprint(runtime);
    runtime.playerSprintFading = runtime.playerSprintActive = false;
    runtime.jumpLaunching = runtime.jumpFromLaunch = runtime.jumpHorizontalValid = false;
    runtime.jumpButtonHeld = false;
    runtime.jumpLaunchTime = 0;
    runtime.jumpHorizontalVelocity = {};
    runtime.verticalSpeed = runtime.airborneTime = runtime.landingTime = runtime.movementSpeed = 0;
    runtime.playerFall = {};
    runtime.grounded = false;
    runtime.health = runtime.maximumHealth();
    runtime.armour = 0;
    runtime.wanted = 0;
    runtime.crime = {};
    runtime.crime.vcs = vcs;
    runtime.arrestProgress = 0;
    runtime.arrestOfficer = -1;
    runtime.visibleArea = 0;
    runtime.entryTransition = -1;
    runtime.entryFade = runtime.entryTime = runtime.entryCooldown = 0;
    runtime.entryNeedsLeave = false;
    runtime.statistics["KillsSinceLastCheckpoint"] = 0;
    world.statistics["KillsSinceLastCheckpoint"] = 0;
    storylandClearPlayTestTarget(runtime);
    runtime.playerPosition = point.position;
    runtime.playerControl = false;
    world.x = point.position.x;
    world.y = point.position.y;
    world.z = point.position.z;
    world.heading = point.heading - 90.0f;
    world.health = runtime.health;
    world.armour = 0;
    world.wanted = 0;
    world.area = 0;
    world.inVehicle = false;
    world.occupiedMissionVehicle = -1;
    world.playerPassengerSeat = -1;
    world.playerAttachedVehicle = -1;
    world.vcsCharacterObjectParents.erase(0);
    world.vcsCharacterAttachments.erase(0);
    world.playerCollisionEnabled = true;
    world.playerAttachment = {};
    world.playerPositionFrozen = false;
    world.playerVisible = true;
    world.playerRenderAlpha = 255;
    world.playerWarped = true;
    world.playerCrouch = {};
    world.manualAim = {};
    world.playerWaitState = 0;
    world.playerWaitDeadline = 0;
    world.playerVehicleTransition = false;
    world.playerExitVehicleRequested = false;
    world.playerSprintEnergy = std::max(0.0f,world.playerSprintMaximum);
    world.playerDamageProofs = 0;
    world.playerCutsceneSafe = false;
    world.policeIgnorePlayer = false;
    runtime.policeIgnorePlayer = false;
    world.everyoneIgnoresPlayer = false;
    world.drunkInputDelay = 0;
    world.drunkCameraIntensity = world.drunkVisualIntensity = 0;
    world.drunkVisualPeriod = 0;
    world.camera.active = false;
    world.camera.cutsceneOwned = false;
    world.camera.transitioning = false;
    world.camera.restoreRequested = true;
    world.cameraControlEnabled = true;
    world.widescreen = false;
    world.control = false;
    world.criticalMissionRestartRequested = false;
    world.messageKey.clear();
    world.messageNumbers.clear();
    world.messageMilliseconds = 0;
    world.messageStringKey.clear();
    runtime.missionText.clear();
}
}

bool storylandPrepareIntroMissionRestart(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,const StorylandNativePoint& origin,bool vcs,
    std::string& error) {
    if(!vcs || world.introMissionSlot < 0 || !world.introMissionRestartQueued ||
        !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)) {
        error = "Intro restart requires its native mission and a finite player position.";
        return false;
    }
    const bool validCheckpoint=world.introRestartPositionValid &&
        std::isfinite(world.introRestartPosition[0]) &&
        std::isfinite(world.introRestartPosition[1]) &&
        std::isfinite(world.introRestartPosition[2]) &&
        std::isfinite(world.introRestartHeading);
    const StorylandNativePoint checkpoint=validCheckpoint ? StorylandNativePoint{
        world.introRestartPosition[0],world.introRestartPosition[1],world.introRestartPosition[2]} : origin;
    const float heading=validCheckpoint ? world.introRestartHeading : world.heading;
    const StorylandPlayerRestartPoint point{checkpoint,heading + 90.0f,false};
    storylandClearRestartTaxi(runtime,world,runtime.restartTaxi);
    storylandResetPlayerRestartBody(runtime,world,point,vcs);
    runtime.playerRestart = {};
    runtime.wastedActive = runtime.busted = false;
    runtime.arrestWanted = 0;
    runtime.playerDeathTime = 0;
    world.playerArrested = false;
    world.restartMissionTaxiDestinationSet = false;
    world.missionResult = {};
    world.introMissionProtected = true;
    world.introMissionRestartQueued = false;
    world.fadeAlpha = world.fadeStartAlpha = world.fadeTargetAlpha = 1;
    world.fadingColor = {0,0,0};
    world.fading = false;
    error.clear();
    return true;
}

bool storylandApplyPlayerRestart(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,std::vector<StorylandNativeTrafficActor>& vehicles,
    StorylandPlayerRestartState& state,const StorylandNativePoint& origin,bool vcs,
    bool advanceClock,StorylandPlayerRestartPoint& point,std::string& error) {
    if(!state.active() || state.resurrected || state.elapsedMilliseconds < 4096) {
        error = "Player resurrection requires an active native restart deadline.";
        return false;
    }
    if(state.introMissionRestart || world.introMissionProtected || world.introMissionRestartQueued) {
        error = "Intro recovery requires restarting the native mission.";
        return false;
    }
    if(vcs && world.restartMissionTaxiDestinationSet && !finiteRestart(world.restartMissionTaxiDestination)) {
        error = "Native mission taxi restart has non-finite coordinates.";
        return false;
    }
    if(!world.requestCollision || !world.loadSceneCollision || !world.loadScene) {
        error = "Native player resurrection requires collision and scene streaming bindings.";
        return false;
    }
    const bool nextOverrideActive = world.nextRestartOverrideActive;
    const bool overrideEnabled = world.restartOverrideEnabled;
    if(!storylandChoosePlayerRestart(world,origin,state.cause,vcs,point,error)) return false;
    const auto previousArea = world.area;
    world.area = 0;
    const std::array<float,3> destination{point.position.x,point.position.y,point.position.z + 1.0f};
    const bool resident = world.requestCollision(destination,error) &&
        world.loadSceneCollision(destination,error) && world.loadScene(destination,error);
    world.area = previousArea;
    if(!resident) {
        world.nextRestartOverrideActive = nextOverrideActive;
        world.restartOverrideEnabled = overrideEnabled;
        if(error.empty()) error = "Native player restart destination has not finished loading.";
        return false;
    }
    const auto policy = storylandPlayerRestartPolicy(state.cause,state.wantedAtStart,
        world.freeHealthCare,world.getOutOfJailFree,vcs);
    if(policy.consumeHospitalPass) world.freeHealthCare = false;
    if(policy.consumeJailPass) world.getOutOfJailFree = false;
    runtime.money = std::max(0,runtime.money - policy.fee);
    world.money = runtime.money;
    if(policy.storeReturnableWeapons) {
        runtime.confiscatedWeapons = runtime.inventory;
        runtime.weaponBuybackPosition = point.position;
        runtime.weaponBuybackAvailable = std::any_of(runtime.inventory.begin() +
            std::min(size_t(1),runtime.inventory.size()),runtime.inventory.end(),
            [](const auto& item) { return item.owned && item.ammunition > 0; });
    }
    if(policy.clearWeapons) {
        for(auto& inventory : runtime.inventory) inventory = {};
        if(!runtime.inventory.empty()) runtime.inventory.front().owned = true;
        runtime.equippedWeapon = 0;
        world.playerWeapons.clear();
        world.currentWeapon = 0;
        world.clearPlayerWeaponsRequested = true;
        world.weaponInventoryChanged = true;
        world.weaponSelectionRequested = true;
    }
    if(advanceClock && policy.minutesPassed) {
        storylandAdvancePlayTestClock(runtime,float(policy.minutesPassed) / 60.0f);
        world.hour = runtime.clockHour;
        world.statistics["DaysPassed"] = runtime.statistics["DaysPassed"];
        const uint32_t passed = policy.minutesPassed * 1000u;
        for(auto& pickup : world.pickups) {
            // CPickups::PassTime: VCS 0x1E2348 excludes type 12;
            // LCS 0x1CBA20 excludes type 16. Both saturate the timer at zero.
            if(pickup.type == 0 || pickup.type == (vcs ? 12 : 16)) continue;
            pickup.respawnAt = storylandAdvancePickupRestartTimer(pickup.respawnAt,passed,world.gameMilliseconds);
            pickup.expiresAt = storylandAdvancePickupRestartTimer(pickup.expiresAt,passed,world.gameMilliseconds);
        }
        for(auto& pickup : runtime.pickups)
            pickup.respawn = std::max(0.0f,pickup.respawn - float(policy.minutesPassed));
    }
    (void)vehicles;
    point.position.z += 1.0f;
    storylandResetPlayerRestartBody(runtime,world,point,vcs);
    // Native ClearExcitingStuffFromArea is invoked at both the old position
    // and the restart. The shared clear handler preserves mission ownership.
    world.clearAreaEvents.push_back({origin.x,origin.y,origin.z,4000.0f,true});
    world.clearAreaEvents.push_back({point.position.x,point.position.y,point.position.z,4000.0f,true});
    state.resurrected = true;
    if(vcs && (state.cause == StorylandPlayerRestartCause::Death || state.cause == StorylandPlayerRestartCause::Arrest)) {
        std::string taxiError;
        if(!storylandSetUpAfterDeathRestartTaxi(runtime,world,runtime.restartTaxi,point.position,taxiError)) {
            error = taxiError;
            return false;
        }
    } else if(state.cause == StorylandPlayerRestartCause::Critical) world.restartMissionTaxiDestinationSet = false;
    error.clear();
    return true;
}

void storylandReleasePlayerRestartControl(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world) {
    runtime.wastedActive = false;
    runtime.busted = false;
    runtime.arrestWanted = 0;
    runtime.playerDeathTime = 0;
    runtime.playerControl = true;
    world.playerArrested = false;
    world.control = true;
    world.fadingColor = runtime.playerRestart.cause == StorylandPlayerRestartCause::Death ?
        std::array<uint8_t,3>{200,200,200} : std::array<uint8_t,3>{0,0,0};
    world.fadeAlpha = world.fadeStartAlpha = 1;
    world.fadeTargetAlpha = 0;
    world.fadeDirection = 1;
    world.fadeDuration = world.fadeTotal = 3000;
    world.fading = true;
}

void storylandApplyPlayerRestartPresentation(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,const StorylandPlayerRestartState& state) {
    if(!state.active() || state.controlReleased) return;
    const auto frame = storylandPlayerRestartPresentation(state);
    runtime.respawnFade = 0;
    world.fadingColor = frame.fadeColor;
    world.fadeAlpha = world.fadeStartAlpha = world.fadeTargetAlpha = frame.fadeAlpha;
    world.fadeDuration = world.fadeTotal = 0;
    world.fading = false;
}

namespace {
void restartTaxiFade(StorylandMissionEnvironment& world,uint32_t duration,bool in) {
    world.fadingColor = {0,0,0};
    world.fadeDuration = world.fadeTotal = float(duration);
    world.fadeDirection = in ? 1 : 0;
    world.fadeStartAlpha = world.fadeAlpha;
    world.fadeTargetAlpha = in ? 0.0f : 1.0f;
    world.fading = true;
}

void restartTaxiHoldControl(StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment& world,
    StorylandRestartTaxiState& state) {
    if(!state.holdsControl) {
        state.previousPlayerControl = runtime.playerControl;
        state.previousWorldControl = world.control;
        state.holdsControl = true;
    }
    runtime.playerControl = world.control = false;
}

bool restartTaxiDriverReady(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::VehicleScriptState& vehicle,const StorylandRestartTaxiState& state) {
    const auto found = world.missionCharacters.find(state.driver);
    if(vehicle.driver != state.driver || found == world.missionCharacters.end()) return false;
    const auto& driver = found->second;
    if(!driver.alive || driver.health <= 0 || driver.removeFromWorld ||
        driver.vehicle != state.vehicle || driver.behavior.leavingVehicleToDie) return false;
    const auto task = world.characterVehicleTasks.find(state.driver);
    if(task != world.characterVehicleTasks.end() && task->second.exiting) return false;
    for(const auto& fire : world.scriptFires)
        if(fire.second.active && fire.second.attachedKind == 1 && fire.second.attachedHandle == state.driver) return false;
    return true;
}

void restartTaxiCruise(StorylandPlayTestRuntime& runtime,StorylandMissionEnvironment::VehicleScriptState& taxi,
    std::vector<StorylandNativeTrafficActor>& vehicles) {
    if(taxi.driver < 0) return;
    taxi.aiActive = true;
    taxi.aiMission = 1;
    taxi.autoPilotMission = 1;
    taxi.cruiseSpeed = 18.0f * (5.0f / 6.0f);
    if(taxi.spawnedSlot >= 0 && size_t(taxi.spawnedSlot) < vehicles.size())
        storylandJoinMissionCarWithRoad(runtime.livePaths,vehicles[size_t(taxi.spawnedSlot)]);
}
}

void storylandClearRestartTaxi(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandRestartTaxiState& state) {
    const auto found = world.missionVehicles.find(state.vehicle);
    if(found != world.missionVehicles.end()) {
        auto& vehicle = found->second;
        vehicle.persistent = false;
        vehicle.ambientOrigin = true;
    }
    const auto driver = world.missionCharacters.find(state.driver);
    if(driver != world.missionCharacters.end()) {
        driver->second.persistent = false;
        driver->second.creationSource = 1;
    }
    if(state.blip != -1) world.radarBlips.erase(std::remove_if(world.radarBlips.begin(),world.radarBlips.end(),
        [&](const auto& blip) { return blip.handle == state.blip; }),world.radarBlips.end());
    if(state.holdsControl && (!runtime.playerRestart.active() || runtime.playerRestart.controlReleased)) {
        runtime.playerControl = state.previousPlayerControl;
        world.control = state.previousWorldControl;
    }
    state = {};
}

bool storylandSetUpAfterDeathRestartTaxi(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandRestartTaxiState& state,
    const StorylandNativePoint& player,std::string& error) {
    error.clear();
    if(!world.restartMissionTaxiDestinationSet) return true;
    world.restartMissionTaxiDestinationSet = false;
    if(!finiteRestart(world.restartMissionTaxiDestination) || !storylandRadarFinitePoint(player)) {
        error = "Native mission taxi restart has non-finite coordinates.";
        return false;
    }
    const StorylandMissionEnvironment::PoliceRestart* selected = nullptr;
    float nearest = 99999.9f;
    // VCS 0x24E220 searches only registered starts, retaining the first strict tie.
    for(size_t index = 0; index < std::min(size_t(16),world.taxiRestarts.size()); ++index) {
        const auto& start = world.taxiRestarts[index];
        if(!finiteRestart(start)) continue;
        const float dx = start.x - player.x,dy = start.y - player.y,dz = start.z - player.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if(distance < nearest) { nearest = distance; selected = &start; }
    }
    if(!selected || nearest >= 100.0f) return true;
    storylandClearRestartTaxi(runtime,world,state);
    state.phase = StorylandRestartTaxiPhase::AwaitModel;
    state.start = {selected->x,selected->y,selected->z};
    state.startHeading = selected->heading;
    const auto& destination = world.restartMissionTaxiDestination;
    state.destination = {destination.x,destination.y,destination.z};
    state.destinationHeading = destination.heading;
    world.requestedModels.insert(183);
    return true;
}

bool storylandUpdateRestartTaxi(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,std::vector<StorylandNativeTrafficActor>& vehicles,
    StorylandRestartTaxiState& state,StorylandNativePoint& player,float& heading,
    std::string& error) {
    error.clear();
    if(!state.active()) return true;
    if(world.cutscene.playing) return true;
    if(!storylandValidRestartTaxiState(state)) {
        error = "Native mission taxi has invalid saved state.";
        return false;
    }
    if(runtime.health <= 0 || runtime.busted || world.criticalMissionRestartRequested) {
        storylandClearRestartTaxi(runtime,world,state);
        return true;
    }
    using Phase = StorylandRestartTaxiPhase;
    if(state.phase == Phase::AwaitModel) {
        world.requestedModels.insert(183);
        if(!world.residentModels.count(183)) return true;
        const auto definition = std::find_if(runtime.modelDefinitions.begin(),runtime.modelDefinitions.end(),
            [](const auto& model) { return model.modelId == 183; });
        if(definition == runtime.modelDefinitions.end()) return true;
        if(world.missionVehicles.size() >= 128 || world.missionCharacters.size() >= 128 ||
            world.nextMissionVehicleHandle <= 0 || world.nextMissionVehicleHandle == INT32_MAX ||
            world.nextMissionCharacterHandle <= 0 || world.nextMissionCharacterHandle == INT32_MAX) return true;
        const size_t driverModel = storylandChooseVehicleDriver(StorylandServiceRole::Civilian,definition->vehicleClass,
            runtime.pedestrianModelStats,runtime.pedestrianModelRoles,world.nextNativeRandom(),
            &runtime.lcsPedestrianGroups);
        if(driverModel == SIZE_MAX) return true;
        const auto& stats = runtime.pedestrianModelStats[driverModel];
        StorylandMissionEnvironment::VehicleScriptState taxi;
        taxi.x = state.start.x; taxi.y = state.start.y; taxi.z = state.start.z;
        taxi.heading = state.startHeading;
        taxi.model = 183;
        taxi.scriptStatus = 3;
        taxi.aiActive = true;
        taxi.aiMission = 0;
        taxi.autoPilotMission = 11;
        taxi.cruiseSpeed = 0;
        taxi.driver = world.nextMissionCharacterHandle;
        if(world.groundHeightAt && definition->hasNativeSuspension) {
            float ground = 0;
            if(world.groundHeightAt({taxi.x,taxi.y,taxi.z + 1.0f},ground) && std::isfinite(ground))
                taxi.z = ground + definition->nativeRideHeight;
        }
        StorylandMissionEnvironment::Character driver;
        driver.model = int32_t(stats.modelId); driver.type = stats.pedType;
        driver.pedStats = stats; driver.pedStat = stats.statIndex;
        driver.animationGroup = stats.animationGroup;
        driver.x = taxi.x; driver.y = taxi.y; driver.z = taxi.z; driver.heading = taxi.heading;
        driver.vehicle = world.nextMissionVehicleHandle;
        driver.passengerSeat = -1;
        driver.creationSource = 2;
        driver.speechProfile = world.nativePedSpeechProfile ? world.nativePedSpeechProfile(driver.model) : -1;
        state.vehicle = world.nextMissionVehicleHandle++;
        state.driver = world.nextMissionCharacterHandle++;
        world.missionVehicles.emplace(state.vehicle,std::move(taxi));
        world.missionCharacters.emplace(state.driver,std::move(driver));
        world.requestedModels.insert(int32_t(stats.modelId));
        state.blip = storylandAllocateMissionBlipHandle(world,true);
        if(state.blip != -1) {
            StorylandMissionEnvironment::RadarBlip blip;
            blip.handle = state.blip; blip.entity = StorylandMissionEnvironment::BlipEntity::Vehicle;
            blip.entityHandle = state.vehicle; blip.color = 0; blip.display = 2;
            world.radarBlips.push_back(blip);
        }
        state.phase = Phase::Waiting;
        return true;
    }
    const auto found = world.missionVehicles.find(state.vehicle);
    if(found == world.missionVehicles.end() || found->second.deleted) {
        storylandClearRestartTaxi(runtime,world,state);
        return true;
    }
    auto& taxi = found->second;
    const int32_t slot = taxi.spawnedSlot;
    const bool spawned = slot >= 0 && size_t(slot) < vehicles.size() && size_t(slot) < runtime.vehicles.size() &&
        runtime.vehicles[size_t(slot)].scriptHandle == state.vehicle;
    if(!spawned && state.phase != Phase::Waiting) {
        storylandClearRestartTaxi(runtime,world,state);
        return true;
    }
    const bool aboard = runtime.occupiedVehicle == slot && spawned && runtime.playerVehicleSeat >= 0;
    const bool damaged = taxi.health < taxi.maximumHealth || taxi.wrecked || !taxi.alive ||
        (spawned && (runtime.vehicles[size_t(slot)].health < taxi.maximumHealth || runtime.vehicles[size_t(slot)].wrecked));
    if(state.phase != Phase::FadingOut && (!restartTaxiDriverReady(world,taxi,state) || damaged)) {
        if(aboard) world.playerExitVehicleRequested = true;
        restartTaxiCruise(runtime,taxi,vehicles);
        storylandClearRestartTaxi(runtime,world,state);
        return true;
    }
    if(state.holdsControl) runtime.playerControl = world.control = false;
    if(state.phase == Phase::Waiting) {
        const float dx = taxi.x - player.x,dy = taxi.y - player.y,dz = taxi.z - player.z;
        if(dx * dx + dy * dy + dz * dz > 200.0f * 200.0f) {
            restartTaxiCruise(runtime,taxi,vehicles);
            storylandClearRestartTaxi(runtime,world,state);
            return true;
        }
        if(spawned && runtime.transitionVehicle == slot &&
            (runtime.vehicleAction == StorylandVehicleAction::Enter || runtime.vehicleAction == StorylandVehicleAction::Hijack) &&
            storylandBeginPlayTestVehicleEntry(runtime,vehicles,player,slot,0)) {
            restartTaxiHoldControl(runtime,world,state);
            state.phase = Phase::Entering;
        }
    } else if(state.phase == Phase::Entering) {
        if(aboard) {
            taxi.temporaryAction = 8;
            taxi.temporaryActionUntil = world.gameMilliseconds + 2500;
            restartTaxiFade(world,2500,false);
            state.deadline = world.gameMilliseconds + 3000;
            world.messageKey = "TAXI"; world.messageStyle = 6; world.messageMilliseconds = 4500;
            world.messageNumbers.clear(); world.messageStringKey.clear();
            state.phase = Phase::FadingOut;
        } else if(runtime.vehicleAction == StorylandVehicleAction::None) {
            storylandClearRestartTaxi(runtime,world,state);
        }
    } else if(state.phase == Phase::FadingOut && int32_t(world.gameMilliseconds - state.deadline) > 0) {
        const std::array<float,3> destination{state.destination.x,state.destination.y,state.destination.z};
        if(!world.requestCollision || !world.loadSceneCollision || !world.loadScene || !world.clearVehicleSpawnSpace) {
            error = "Native mission taxi arrival requires collision and scene streaming bindings.";
            return false;
        }
        if(!world.requestCollision(destination,error) || !world.loadSceneCollision(destination,error) || !world.loadScene(destination,error)) return false;
        taxi.x = state.destination.x; taxi.y = state.destination.y; taxi.z = state.destination.z;
        taxi.heading = state.destinationHeading;
        taxi.movedByScript = taxi.headingChangedByScript = true;
        const auto& definition = runtime.vehicles[size_t(slot)].nativeDefinition;
        if(world.groundHeightAt && definition.hasNativeSuspension) {
            float ground = 0;
            if(world.groundHeightAt({taxi.x,taxi.y,taxi.z + 1.0f},ground) && std::isfinite(ground))
                taxi.z = ground + definition.nativeRideHeight;
        }
        world.clearVehicleSpawnSpace(state.vehicle);
        const int32_t arrivalSlot = taxi.spawnedSlot;
        if(arrivalSlot < 0 || size_t(arrivalSlot) >= vehicles.size() ||
            size_t(arrivalSlot) >= runtime.vehicles.size() ||
            runtime.vehicles[size_t(arrivalSlot)].scriptHandle != state.vehicle) {
            error = "Native mission taxi lost its physical vehicle during arrival clearing.";
            return false;
        }
        auto& actor = vehicles[size_t(arrivalSlot)];
        actor.position = {taxi.x,taxi.y,taxi.z}; actor.yaw = taxi.heading + 90;
        auto& vehicle = runtime.vehicles[size_t(arrivalSlot)];
        vehicle.speed = 20; vehicle.lateralSpeed = vehicle.verticalSpeed = 0;
        const float angle = actor.yaw * .017453292519943295f;
        taxi.velocity = {std::cos(angle) * 20,std::sin(angle) * 20,0};
        taxi.velocityChangedByScript = true;
        player = actor.position; heading = actor.yaw; runtime.playerPosition = player;
        world.x = player.x; world.y = player.y; world.z = player.z; world.heading = taxi.heading;
        world.camera.restoreRequested = true;
        state.deadline = world.gameMilliseconds + 500;
        restartTaxiFade(world,1500,true);
        state.phase = Phase::Arriving;
    } else if(state.phase == Phase::Arriving && int32_t(world.gameMilliseconds - state.deadline) > 0) {
        runtime.money = std::max(0,runtime.money - 9); world.money = runtime.money;
        world.playerExitVehicleRequested = true;
        state.phase = Phase::Exiting;
    } else if(state.phase == Phase::Exiting && !aboard && runtime.vehicleAction != StorylandVehicleAction::Exit) {
        restartTaxiCruise(runtime,taxi,vehicles);
        storylandClearRestartTaxi(runtime,world,state);
    }
    return true;
}
