#pragma once
#include "src/storyland/storyland_coop_authority.h"
#include "src/storyland/storyland_character_sound_events.h"
#include <cstring>
#include <deque>
#include <vector>

// Mission actor replication over the ordered, authenticated game channel.
// This owns protocol state, not the script VM, rendering or ambient population.
// The application must suspend guest simulation before applying its replicas.
class StorylandCoopSync {
public:
    enum class Phase : uint32_t { FreeRoam, Running, Passed, Failed };
    enum class Kind : uint32_t { Civilian, Police, Enemy };
    struct Actor {
        StorylandCoopAuthority::Entity identity;
        StorylandNativePoint position{};
        float heading = 0, health = 100, armour = 0, movementSpeed = 0;
        int32_t animationGroup = 0, animation = -1, weapon = 0, objective = 0;
        int32_t objectiveTarget = -1, lastWeaponDamage = -1, damageSourceHandle = -1;
        uint32_t damageSourceKind = 0;
        int32_t ammunition = 0, magazine = 0;
        float reloading = 0;
        bool objectiveCompleted = false;
        uint32_t animationStartedAt = 0, deathStartedAt = 0, shotSerial = 0, sampledAt = 0;
        float walkedDistance = 0, walkSpeed = 1.25f;
        int32_t vehicle = -1, passengerSeat = -1;
        uint32_t movementState = 0;
        bool alive = true, visible = true, animationLoop = false, walking = false, deathRecorded = false;
        Kind kind = Kind::Civilian;
        int32_t targetPlayer = -1;
    };
    std::function<void(uint64_t, int32_t, Phase, const std::string&, StorylandMissionFailure)> onMissionResult;
    std::function<void(int32_t, bool)> onMissionBegin;
    std::function<bool(const StorylandCoopPlayerHit&)> onPlayerHit;
    std::function<void(float, const std::vector<StorylandRuntimeParticle>&)> onParticles;
    bool hitPlayer(const StorylandCoopPlayerHit& hit) {
        if (!epoch || !room() || !validPlayerHit(hit)) return false;
        Writer message(Type::PeerHit, epoch, ++sent);
        message.i32(hit.weapon); message.u32(hit.kind); message.u32(hit.shot); message.u32(hit.targetLife);
        message.number(hit.attackTime);
        point(message,hit.origin); point(message,hit.impact);
        message.u32(hit.hitPiece); message.u32(hit.hitDirection);
        message.i32(hit.explosionType);
        outgoing.push_back(std::move(message.bytes)); return true;
    }
    bool publishParticles(float sampledAt,const std::vector<StorylandRuntimeParticle>& particles) {
        if (!epoch || !room() || particles.empty() || particles.size()>8 || !bounded(sampledAt,0,1e8f)) return false;
        for(const auto& particle:particles)if(!validParticle(particle))return false;
        Writer message(Type::Particles,epoch,++sent); message.number(sampledAt); message.u32(uint32_t(particles.size()));
        for(const auto& p:particles) {
            point(message,p.position); point(message,p.velocity); message.u32(uint32_t(p.kind));
            message.number(p.remaining); message.number(p.lifetime); message.number(p.size);
            message.i32(p.nativeType); message.u32(p.nativeFrame); message.u32(p.nativeLastFrame);
            message.u32(p.nativeFrameThreshold); message.u32(p.nativePhysics?1:0); message.u32(p.nativeFrameLoop?1:0);
            message.number(p.nativeExpansion); message.number(p.nativeGravity); message.u32(p.nativeFriction);
            for(auto c:p.nativeColor)message.u32(c);
        }
        outgoing.push_back(std::move(message.bytes)); return true;
    }
    struct VehicleEvent {
        enum class Action : uint32_t { Request, Grant, Take, Reject, Garage };
        Action action = Action::Request;
        uint32_t identity = 0, life = 0;
        int32_t model = -1, seat = -1, garage = -1;
        StorylandNativePoint position{};
    };
    std::function<bool(const VehicleEvent&)> onVehicleEvent;
    bool vehicleEvent(const VehicleEvent& event) {
        if (!epoch || !room() || !validVehicleEvent(event)) return false;
        Writer message(Type::Vehicle, epoch, ++sent);
        message.u32(uint32_t(event.action)); message.u32(event.identity); message.u32(event.life);
        message.i32(event.model); message.i32(event.seat); message.i32(event.garage);
        point(message, event.position);
        outgoing.push_back(std::move(message.bytes));
        return true;
    }
    using Validator = std::function<bool(unsigned, const StorylandMissionCharacterDamage&)>;
    static constexpr size_t MaximumActors = 2048;
    static constexpr size_t MaximumPending = 64;

    StorylandCoopSync(StorylandPlayTestRuntime& runtime, bool host)
        : runtime(runtime), authority(runtime), host(host) {}
    StorylandCoopSync(const StorylandCoopSync&) = delete;
    StorylandCoopSync& operator=(const StorylandCoopSync&) = delete;

    bool sceneReady() const { return completeScene; }
    int npcTarget(const StorylandNativePoint& position, bool police) const {
        return police ? authority.policeTarget(position, 90) : authority.enemyTarget(position, 90, 3);
    }
    bool completeSnapshot() {
        if (!host || !epoch || !room()) return false;
        Writer message(Type::SceneComplete, epoch, ++sent);
        outgoing.push_back(std::move(message.bytes)); completeScene = true; return true;
    }
    bool damageRemotePlayer(float amount, int32_t weapon, uint8_t kind, int32_t attacker) {
        if (!host || !epoch || !room() || !runtime.coopRemoteLife || attacker <= 0 ||
            !bounded(amount, 0.000001f, 100000) || (kind != 1 && kind != 16)) return false;
        Writer message(Type::PlayerDamage, epoch, ++sent);
        message.number(amount); message.i32(weapon); message.u32(kind); message.i32(attacker);
        message.u32(runtime.coopRemoteLife);
        outgoing.push_back(std::move(message.bytes)); return true;
    }
    bool observeCutscene(const std::string& name, bool playing, uint32_t startRequest = 0) {
        if (!host || !epoch) return false;
        if (!playing) { cutscenePlaying = false; return true; }
        if (!validCutsceneName(name)) return false;
        if (cutscenePlaying && name == cutsceneName && startRequest == cutsceneStartRequest) return true;
        if (!room() || cutsceneGeneration == std::numeric_limits<uint64_t>::max()) return false;
        Writer message(Type::CutsceneBegin, epoch, ++sent);
        message.u64(cutsceneGeneration + 1); message.string(name);
        outgoing.push_back(std::move(message.bytes));
        ++cutsceneGeneration; cutsceneName = name; cutscenePlaying = true; cutsceneStartRequest = startRequest;
        cutsceneSkipPending = cutsceneSkipApplied = false;
        return true;
    }
    bool requestCutsceneSkip(const std::string& name) {
        if (!epoch || !cutscenePlaying || name != cutsceneName || !room()) return false;
        if (host && (cutsceneSkipPending || cutsceneSkipApplied)) return true;
        Writer message(host ? Type::CutsceneSkip : Type::CutsceneSkipRequest, epoch, ++sent);
        message.u64(cutsceneGeneration); message.string(name);
        outgoing.push_back(std::move(message.bytes));
        if (host) cutsceneSkipPending = true;
        return true;
    }
    bool takeCutsceneSkip(const std::string& name, bool playing) {
        if (!playing || name != cutsceneName || !cutsceneSkipPending || cutsceneSkipApplied) return false;
        cutsceneSkipPending = false; cutsceneSkipApplied = true;
        return true;
    }
    uint64_t missionId() const { return epoch; }
    int32_t missionIndex() const { return mission; }
    Phase missionPhase() const { return phase; }
    const std::string& resultKey() const { return terminalKey; }
    StorylandMissionFailure resultFailure() const { return terminalFailure; }
    const std::map<int32_t, Actor>& actors() const { return replicas; }
    bool setPlayer(unsigned index, const StorylandCoopAuthority::Player& player) {
        return authority.setPlayer(index, player);
    }

    // Call once for a new mission/free-roam world, not on every simulation tick.
    bool beginMission(int32_t index, Phase state, bool restarted = false) {
        if (!host || !room() || !validMission(index, state) || !authority.beginMission()) return false;
        epoch = authority.missionId(); mission = index; phase = state;
        replicas.clear(); completeScene = false; terminalKey.clear(); terminalFailure = StorylandMissionFailure::None;
        resetCutscene();
        Writer message(Type::Mission, epoch, ++sent);
        message.i32(index); message.u32(uint32_t(state)); message.u32(restarted ? 1u : 0u);
        outgoing.push_back(std::move(message.bytes));
        return true;
    }
    bool setMissionPhase(Phase state, const std::string& key = {},
        StorylandMissionFailure failure = StorylandMissionFailure::Script) {
        if (!host || !epoch || !room() || !transition(phase, state) || key.size() > 64 ||
            unsigned(failure) > unsigned(StorylandMissionFailure::Arrest)) return false;
        terminalFailure = state == Phase::Failed ? (failure == StorylandMissionFailure::None ?
            StorylandMissionFailure::Script : failure) : StorylandMissionFailure::None;
        terminalKey = key;
        Writer message(Type::Phase, epoch, ++sent);
        message.u32(uint32_t(state)); message.u32(uint32_t(terminalFailure)); message.string(key);
        outgoing.push_back(std::move(message.bytes)); phase = state;
        if(onMissionResult)onMissionResult(epoch, mission, phase, terminalKey, terminalFailure);
        return true;
    }
    bool requestMissionPhase(Phase state, const std::string& key, StorylandMissionFailure failure) {
        if (host || !epoch || !room() || !transition(phase, state) || key.size() > 64 ||
            unsigned(failure) > unsigned(StorylandMissionFailure::Arrest)) return false;
        Writer message(Type::PhaseRequest, epoch, ++sent);
        message.u32(uint32_t(state)); message.u32(uint32_t(failure)); message.string(key);
        outgoing.push_back(std::move(message.bytes)); return true;
    }

    // Native handles are local lookup keys. Generation prevents their reuse from
    // turning an old hit into damage to a new NPC. Removal must be published before
    // replacing a character with the same handle and model.
    bool publishActor(int32_t handle, Kind kind, unsigned hostilePlayers = 3) {
        if (!host || !epoch || !room() || !runtime.missionEnvironment ||
            uint32_t(kind) > uint32_t(Kind::Enemy) || hostilePlayers > 3) return false;
        const auto found = runtime.missionEnvironment->missionCharacters.find(handle);
        if (found == runtime.missionEnvironment->missionCharacters.end() || handle <= 0) return false;
        if (!replicas.count(handle) && replicas.size() >= MaximumActors) return false;
        const auto& character = found->second;
        Actor actor;
        actor.identity = {epoch, 1, handle, character.model};
        actor.position = {character.x, character.y, character.z};
        actor.heading = character.heading; actor.health = character.health; actor.armour = character.armour;
        actor.movementSpeed = character.movementSpeed;
        actor.animationGroup = character.animationGroup; actor.animation = character.animationId;
        actor.animationStartedAt = character.animationStartedAt; actor.animationLoop = character.animationLoop;
        actor.deathStartedAt = character.deathStartedAt; actor.weapon = character.currentWeapon;
        actor.objective = character.objective; actor.objectiveTarget = character.objectiveTarget;
        actor.objectiveCompleted = character.objectiveCompleted;
        actor.lastWeaponDamage = character.lastWeaponDamage;
        actor.damageSourceKind = uint32_t(character.lastDamageSource.kind); actor.damageSourceHandle = character.lastDamageSource.handle;
        const auto inventory = character.weapons.find(character.currentWeapon);
        if (inventory != character.weapons.end()) actor.ammunition = std::clamp(inventory->second.ammunition, 0, 1000000);
        actor.alive = character.alive; actor.visible = character.visible;
        actor.deathRecorded = character.deathRecorded;
        actor.kind = kind;
        actor.sampledAt = runtime.missionEnvironment->gameMilliseconds;
        actor.walkedDistance = float(character.walkedDistance); actor.walkSpeed = character.walkSpeed;
        // Object-attached LCS actors are sent using their authoritative world
        // position. The existing wire format only represents vehicle handles.
        const bool lcsObjectAttachment = !runtime.crime.vcs && character.attached && character.vehicle < -1;
        actor.vehicle = lcsObjectAttachment ? -1 : character.vehicle;
        actor.passengerSeat = character.passengerSeat;
        actor.movementState = character.movementState; actor.walking = character.walkingToTarget;
        const auto combat = runtime.missionCombat.find(handle);
        if (combat != runtime.missionCombat.end()) {
            actor.shotSerial = combat->second.shotSerial;
            actor.magazine = std::clamp(combat->second.magazine, 0, 1000000); actor.reloading = std::max(0.0f, combat->second.reloading);
        }
        if (actor.alive && actor.health > 0) {
            if (kind == Kind::Police) actor.targetPlayer = authority.policeTarget(actor.position, 90);
            if (kind == Kind::Enemy) actor.targetPlayer = authority.enemyTarget(actor.position, 90, hostilePlayers);
        }
        if (!validActor(actor)) return false;
        const auto* existing = authority.character(handle);
        actor.identity = existing && existing->model == character.model ? *existing : authority.registerCharacter(handle);
        if (!actor.identity.generation) return false;
        queueActor(actor);
        replicas[handle] = actor;
        return true;
    }
    bool removeActor(int32_t handle) {
        if (!host || !room()) return false;
        const auto found = replicas.find(handle);
        if (found == replicas.end()) return false;
        Writer message(Type::Remove, epoch, ++sent);
        identity(message, found->second.identity);
        outgoing.push_back(std::move(message.bytes));
        authority.removeCharacter(handle); replicas.erase(found);
        return true;
    }

    // Requests never select an attacking player. The host binds them to player 2
    // from the authenticated connection, and requires its own action validation.
    bool requestHit(int32_t handle, const StorylandMissionCharacterDamage& damage) {
        if (host || !epoch || !room() || !validDamage(damage)) return false;
        const auto found = replicas.find(handle);
        if (found == replicas.end() || damage.handle != handle || damage.model != found->second.identity.model) return false;
        Writer message(Type::Hit, epoch, ++sent);
        identity(message, found->second.identity);
        writeDamage(message, damage);
        outgoing.push_back(std::move(message.bytes));
        return true;
    }

    // Call only with bytes returned by StorylandMultiplayer::takeGameMessage.
    // A host never accepts scene/mission state from a guest. A guest never applies
    // damage requests. Complete validation precedes any mutation.
    bool receive(const std::vector<uint8_t>& bytes, const Validator& validate = {}) {
        if (bytes.size() > 944) return false;
        Reader reader(bytes);
        if (reader.u32() != Magic || reader.u32() != Version) return false;
        const auto type = Type(reader.u32());
        const uint64_t incomingEpoch = reader.u64(), serial = reader.u64();
        if (!reader.ok || !incomingEpoch || !serial || serial <= received) return false;
        if (type == Type::Vehicle) {
            if (incomingEpoch != epoch) return false;
            VehicleEvent event;
            event.action = VehicleEvent::Action(reader.u32());
            event.identity = reader.u32(); event.life = reader.u32();
            event.model = reader.i32(); event.seat = reader.i32(); event.garage = reader.i32();
            event.position = readPoint(reader);
            if (!reader.done() || !validVehicleEvent(event) || !onVehicleEvent || !onVehicleEvent(event)) return false;
            received = serial;
            return true;
        }
        if (type == Type::PeerHit) {
            if(incomingEpoch!=epoch)return false;
            StorylandCoopPlayerHit hit;
            hit.weapon=reader.i32(); hit.kind=reader.u32(); hit.shot=reader.u32(); hit.targetLife=reader.u32();
            hit.attackTime=reader.number(); hit.origin=readPoint(reader); hit.impact=readPoint(reader);
            const uint32_t piece=reader.u32(), direction=reader.u32();
            if(piece>6||direction>3)return false;
            hit.hitPiece=uint8_t(piece);hit.hitDirection=uint8_t(direction);
            hit.explosionType=reader.i32();
            if(!reader.done() || !validPlayerHit(hit) || !onPlayerHit || !onPlayerHit(hit))return false;
            received=serial; return true;
        }
        if (type == Type::Particles) {
            if(incomingEpoch!=epoch)return false;
            const float sampledAt=reader.number(); const uint32_t count=reader.u32();
            if(!bounded(sampledAt,0,1e8f)||count==0||count>8)return false;
            std::vector<StorylandRuntimeParticle> particles;
            for(uint32_t i=0;i<count;++i) {
                StorylandRuntimeParticle p; p.position=readPoint(reader); p.velocity=readPoint(reader);
                p.kind=StorylandParticleKind(reader.u32()); p.remaining=reader.number(); p.lifetime=reader.number(); p.size=reader.number();
                p.nativeType=reader.i32(); const auto frame=reader.u32(),last=reader.u32(),threshold=reader.u32();
                const auto physics=reader.u32(),loop=reader.u32();
                if(frame>255||last>255||threshold>65535||physics>1||loop>1)return false;
                p.nativeFrame=uint8_t(frame);p.nativeLastFrame=uint8_t(last);p.nativeFrameThreshold=uint16_t(threshold);
                p.nativePhysics=physics!=0;p.nativeFrameLoop=loop!=0;
                p.nativeExpansion=reader.number();p.nativeGravity=reader.number();p.nativeFriction=reader.u32();
                for(auto& c:p.nativeColor){const auto value=reader.u32();if(value>255)return false;c=uint8_t(value);}
                if(!validParticle(p))return false;
                p.nativeVcs=runtime.crime.vcs;
                if(p.nativePhysics&&runtime.readNativeParticleDescriptor) {
                    StorylandNativeParticleDescriptor descriptor;
                    if(runtime.readNativeParticleDescriptor(p.nativeType,descriptor)) {
                        storylandInitializeNativeParticlePresentation(p,descriptor);
                        storylandAdvanceNativeParticlePresentation(p,std::max(0.0f,p.lifetime-p.remaining));
                    }
                }
                if(p.nativeVcs&&p.kind==StorylandParticleKind::Water&&p.nativeType>=75&&p.nativeType<=77) {
                    p.swimmingParticle=true;p.waterSurface=p.nativeType!=75;
                    p.waterAnchoredSplash=p.nativeType==75&&p.velocity.x==0&&p.velocity.y==0&&p.velocity.z==0;
                    p.waterHeading=std::atan2(p.velocity.y,p.velocity.x);
                    if(p.waterSurface||p.waterAnchoredSplash)p.nativeGravity=0;
                }
                p.coopSent=true;particles.push_back(p);
            }
            if(!reader.done())return false;
            if(onParticles)onParticles(sampledAt,particles);
            received=serial;return true;
        }
        if (host) {
            if (incomingEpoch != epoch || !room()) return false;
            if (type == Type::CutsceneSkipRequest) {
                const uint64_t generation = reader.u64(); const auto name = reader.string();
                if (!reader.done() || generation != cutsceneGeneration || !cutscenePlaying ||
                    name != cutsceneName || !requestCutsceneSkip(name)) return false;
                received = serial; return true;
            }
            if (type == Type::PhaseRequest) {
                const auto state = Phase(reader.u32());
                const auto failure = reader.u32();
                const auto key = reader.string();
                if (!reader.done() || failure > 3 || !setMissionPhase(state, key, StorylandMissionFailure(failure))) return false;
                received = serial; return true;
            }
            if (type != Type::Hit) return false;
            StorylandCoopAuthority::Hit hit;
            hit.sequence = serial; hit.player = 1; hit.target = readIdentity(reader, incomingEpoch);
            hit.damage = readDamage(reader);
            if (!reader.done() || !validDamage(hit.damage)) return false;
            const auto found = replicas.find(hit.target.handle);
            if (found == replicas.end() || !authority.applyHit(hit, validate)) return false;
            // Send the native result, never subtract the requested amount on the guest:
            // armour, proof flags, critical hits and deaths are evaluated once on host.
            Actor result = found->second;
            const auto& character = runtime.missionEnvironment->missionCharacters.at(hit.target.handle);
            result.health = character.health; result.armour = character.armour; result.alive = character.alive;
            result.deathStartedAt = character.deathStartedAt;
            result.animation = character.animationId; result.animationGroup = character.animationGroup;
            result.animationStartedAt = character.animationStartedAt; result.animationLoop = character.animationLoop;
            result.deathRecorded = character.deathRecorded;
            result.sampledAt = runtime.missionEnvironment->gameMilliseconds;
            result.movementSpeed = character.movementSpeed; result.movementState = character.movementState;
            result.walking = character.walkingToTarget;
            result.lastWeaponDamage = character.lastWeaponDamage;
            result.damageSourceKind = uint32_t(character.lastDamageSource.kind); result.damageSourceHandle = character.lastDamageSource.handle;
            if (!result.alive || result.health <= 0) result.targetPlayer = -1;
            queueActor(result); found->second = result;
        } else if (type == Type::Mission) {
            const int32_t index = reader.i32(); const auto state = Phase(reader.u32());
            const uint32_t restarted = reader.u32();
            if (!reader.done() || restarted > 1 || incomingEpoch <= epoch || !validMission(index, state)) return false;
            epoch = incomingEpoch; mission = index; phase = state; replicas.clear(); completeScene = false;
            terminalKey.clear(); terminalFailure = StorylandMissionFailure::None;
            resetCutscene();
            if(onMissionBegin)onMissionBegin(index, restarted != 0);
        } else {
            if (incomingEpoch != epoch) return false;
            if (type == Type::CutsceneBegin) {
                const uint64_t generation = reader.u64(); const auto name = reader.string();
                if (!reader.done() || generation <= cutsceneGeneration || !validCutsceneName(name)) return false;
                cutsceneGeneration = generation; cutsceneName = name; cutscenePlaying = true;
                cutsceneSkipPending = cutsceneSkipApplied = false;
            } else if (type == Type::CutsceneSkip) {
                const uint64_t generation = reader.u64(); const auto name = reader.string();
                if (!reader.done() || generation != cutsceneGeneration || name != cutsceneName ||
                    !validCutsceneName(name)) return false;
                if (!cutsceneSkipApplied) cutsceneSkipPending = true;
            } else if (type == Type::SceneComplete) {
                if (!reader.done()) return false;
                completeScene = true;
            } else if (type == Type::PlayerDamage) {
                const float amount = reader.number(); const int32_t weapon = reader.i32(); const uint32_t kind = reader.u32();
                const int32_t attacker = reader.i32(); const uint32_t targetLife = reader.u32();
                if (attacker <= 0 || !reader.done() || !bounded(amount, 0.000001f, 100000) || (kind != 1 && kind != 16) ||
                    !targetLife || !runtime.missionEnvironment) return false;
                auto& world = *runtime.missionEnvironment;
                if (targetLife == runtime.coopLife && runtime.health > 0 && !runtime.busted) {
                    storylandRequestWeaponHitSpeech(runtime,-1,runtime.playerPosition,weapon);
                    if (runtime.inflictDamage(amount, uint8_t(kind), weapon)) {
                        world.playerLastWeaponDamage = weapon;
                        world.playerDamageSource = {StorylandMissionEnvironment::DamageSourceKind::Character,attacker};
                    }
                }
            } else if (type == Type::Phase) {
                const auto state = Phase(reader.u32());
                const uint32_t failure = reader.u32(); const auto key = reader.string();
                if (!reader.done() || !transition(phase, state) || failure > 3 ||
                    ((state == Phase::Failed) != (failure != 0))) return false;
                phase = state; terminalKey = key; terminalFailure = StorylandMissionFailure(failure);
                if(onMissionResult)onMissionResult(epoch, mission, phase, terminalKey, terminalFailure);
            } else if (type == Type::Actor) {
                Actor actor = readActor(reader, incomingEpoch);
                if (!reader.done() || !validActor(actor)) return false;
                const auto found = replicas.find(actor.identity.handle);
                if (found == replicas.end()) {
                    if (replicas.size() >= MaximumActors) return false;
                } else if (actor.identity.generation < found->second.identity.generation ||
                    (actor.identity.generation == found->second.identity.generation &&
                        actor.identity.model != found->second.identity.model)) return false;
                replicas[actor.identity.handle] = actor;
            } else if (type == Type::Remove) {
                const auto actor = readIdentity(reader, incomingEpoch);
                const auto found = replicas.find(actor.handle);
                if (!reader.done() || found == replicas.end() ||
                    found->second.identity.generation != actor.generation || found->second.identity.model != actor.model) return false;
                replicas.erase(found);
            } else return false;
        }
        received = serial;
        return true;
    }

    // Leave a refused message queued when the reliable transport is full.
    // This bounds memory and allows the caller to retry on a later frame.
    void flush(const std::function<bool(const std::vector<uint8_t>&)>& send) {
        if (!send) return;
        while (!outgoing.empty() && send(outgoing.front())) outgoing.pop_front();
    }
    size_t pending() const { return outgoing.size(); }
    struct PumpResult { unsigned accepted = 0, rejected = 0; };
    // Keep one instance per connection. Recreate it after disconnect so no old
    // mission, actor or queued hit can enter a new peer's session.
    template<class Network> PumpResult pump(Network& network, const Validator& validate = {}) {
        PumpResult result;
        if (!network.connected() || network.isHost() != host) return result;
        const auto send = [&](const auto& bytes) { return network.sendGameMessage(bytes); };
        flush(send);
        std::vector<uint8_t> message;
        // Do not remove a request from the transport until there is room for the
        // native result. Queue pressure must never silently discard accepted hits.
        for (unsigned count = 0; count < 16 && (!host || room()); ++count) {
            if (!network.takeGameMessage(message)) break;
            if (receive(message, validate)) ++result.accepted;
            else ++result.rejected;
        }
        flush(send);
        return result;
    }

private:
    enum class Type : uint32_t { Mission = 1, Phase, Actor, Remove, Hit, PhaseRequest, SceneComplete,
        PlayerDamage, PeerHit, Particles, CutsceneBegin, CutsceneSkip, CutsceneSkipRequest, Vehicle };
    static constexpr uint32_t Magic = 0x5354434d, Version = 5;
    struct Writer {
        std::vector<uint8_t> bytes;
        Writer(Type type, uint64_t epoch, uint64_t serial) {
            bytes.reserve(160); u32(Magic); u32(Version); u32(uint32_t(type)); u64(epoch); u64(serial);
        }
        void u32(uint32_t value) { for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(uint8_t(value >> shift)); }
        void string(const std::string& value) { u32(uint32_t(value.size())); bytes.insert(bytes.end(), value.begin(), value.end()); }
        void u64(uint64_t value) { u32(uint32_t(value)); u32(uint32_t(value >> 32)); }
        void i32(int32_t value) { u32(uint32_t(value)); }
        void number(float value) { uint32_t bits; std::memcpy(&bits, &value, 4); u32(bits); }
    };
    struct Reader {
        const std::vector<uint8_t>& bytes; size_t offset = 0; bool ok = true;
        explicit Reader(const std::vector<uint8_t>& bytes) : bytes(bytes) {}
        uint32_t u32() {
            if (!ok || bytes.size() - offset < 4) { ok = false; return 0; }
            uint32_t value = 0;
            for (unsigned shift = 0; shift < 32; shift += 8) value |= uint32_t(bytes[offset++]) << shift;
            return value;
        }
        std::string string() {
            const uint32_t size = u32();
            if (!ok || size > 64 || size > bytes.size() - offset) { ok = false; return {}; }
            std::string value(bytes.begin() + offset, bytes.begin() + offset + size); offset += size;
            for (unsigned char c : value) if (c < 32 || c > 126) ok = false;
            return value;
        }
        uint64_t u64() { const uint64_t low = u32(); return low | (uint64_t(u32()) << 32); }
        int32_t i32() { const uint32_t bits = u32(); int32_t value; std::memcpy(&value, &bits, 4); return value; }
        float number() { const uint32_t bits = u32(); float value; std::memcpy(&value, &bits, 4); return value; }
        bool done() const { return ok && offset == bytes.size(); }
    };
    static void identity(Writer& writer, const StorylandCoopAuthority::Entity& actor) {
        writer.u64(actor.generation); writer.i32(actor.handle); writer.i32(actor.model);
    }
    static StorylandCoopAuthority::Entity readIdentity(Reader& reader, uint64_t epoch) {
        StorylandCoopAuthority::Entity result;
        result.mission = epoch; result.generation = reader.u64(); result.handle = reader.i32(); result.model = reader.i32();
        return result;
    }
    static void point(Writer& w,const StorylandNativePoint& p) { w.number(p.x); w.number(p.y); w.number(p.z); }
    static StorylandNativePoint readPoint(Reader& r) { StorylandNativePoint p; p.x=r.number(); p.y=r.number(); p.z=r.number(); return p; }
    static bool validPoint(const StorylandNativePoint& p,float limit=100000) {
        return bounded(p.x,-limit,limit)&&bounded(p.y,-limit,limit)&&bounded(p.z,-limit,limit);
    }
    static bool validVehicleEvent(const VehicleEvent& event) {
        return uint32_t(event.action) <= uint32_t(VehicleEvent::Action::Garage) && event.identity &&
            event.life && event.model >= 0 && event.model <= 100000 && event.seat >= -1 &&
            event.seat <= 2 && event.garage >= -1 && event.garage <= 100000 && validPoint(event.position);
    }
    static bool validPlayerHit(const StorylandCoopPlayerHit& h) {
        return h.weapon>=0&&h.weapon<=255&&(h.kind==1||h.kind==16||h.kind==4)&&h.shot&&h.targetLife&&
            (h.kind==4 ? (h.explosionType>=0&&h.explosionType<=11&&h.explosionType!=1) : h.explosionType==-1)&&
            bounded(h.attackTime,0,1e8f)&&validPoint(h.origin)&&validPoint(h.impact)&&h.hitPiece<=6&&h.hitDirection<=3;
    }
    static bool validParticle(const StorylandRuntimeParticle& p) {
        return validPoint(p.position)&&validPoint(p.velocity,1000)&&uint32_t(p.kind)<=uint32_t(StorylandParticleKind::Dollar)&&
            bounded(p.remaining,.000001f,60)&&bounded(p.lifetime,p.remaining,60)&&bounded(p.size,.000001f,100)&&
            p.nativeType>=-1&&p.nativeType<90&&bounded(p.nativeExpansion,-100,100)&&bounded(p.nativeGravity,-100,100)&&p.nativeFriction<=10000;
    }
    static bool validMission(int32_t index, Phase state) {
        return (state == Phase::FreeRoam && index == -1) || (state == Phase::Running && index >= 0);
    }
    static bool validCutsceneName(const std::string& name) {
        return !name.empty() && name.size() <= 24 && std::all_of(name.begin(), name.end(), [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '_';
        });
    }
    void resetCutscene() {
        cutsceneName.clear(); cutsceneGeneration = 0; cutsceneStartRequest = 0; cutscenePlaying = false;
        cutsceneSkipPending = cutsceneSkipApplied = false;
    }
    static bool transition(Phase from, Phase to) {
        return from == Phase::Running && (to == Phase::Passed || to == Phase::Failed);
    }
    static bool bounded(float value, float lower, float upper) {
        return std::isfinite(value) && value >= lower && value <= upper;
    }
    static bool validActor(const Actor& actor) {
        return actor.identity.mission && actor.identity.generation && actor.identity.handle > 0 && actor.identity.model >= 0 &&
            bounded(actor.position.x, -1000000, 1000000) && bounded(actor.position.y, -1000000, 1000000) &&
            bounded(actor.position.z, -1000000, 1000000) && bounded(actor.heading, -360000, 360000) &&
            bounded(actor.health, 0, 100000) && bounded(actor.armour, 0, 100000) &&
            bounded(actor.movementSpeed, 0, 10000) && bounded(actor.walkedDistance, 0, 1.0e12f) &&
            bounded(actor.walkSpeed, 0, 10000) && actor.movementState <= 4 && uint32_t(actor.kind) <= uint32_t(Kind::Enemy) &&
            actor.targetPlayer >= -1 && actor.targetPlayer <= 1 && actor.damageSourceKind <= 3 &&
            actor.vehicle >= -1 && actor.passengerSeat >= -1 && actor.passengerSeat <= 7 &&
            actor.ammunition >= 0 && actor.ammunition <= 1000000 && actor.magazine >= 0 && actor.magazine <= 1000000 &&
            bounded(actor.reloading, 0, 100000);
    }
    static bool validDamage(const StorylandMissionCharacterDamage& damage) {
        return damage.handle > 0 && damage.model >= 0 && bounded(damage.amount, 0.000001f, 100000) &&
            (damage.kind == 0 || damage.kind == 1 || damage.kind == 2 || damage.kind == 4 || damage.kind == 8 || damage.kind == 16) &&
            damage.playerAttack && damage.sourceKind == 1 && damage.sourceHandle == 0 &&
            bounded(damage.attackTime, 0, 100000000) && damage.hitPiece <= 6 && damage.hitDirection <= 3;
    }
    static void writeDamage(Writer& writer, const StorylandMissionCharacterDamage& damage) {
        writer.i32(damage.handle); writer.i32(damage.model); writer.number(damage.amount);
        writer.u32(damage.kind); writer.i32(damage.weapon);
        writer.u32((damage.headHit ? 1u : 0u) | (damage.critical ? 2u : 0u));
        writer.number(damage.attackTime);
        writer.u32(damage.hitPiece); writer.u32(damage.hitDirection);
    }
    static StorylandMissionCharacterDamage readDamage(Reader& reader) {
        StorylandMissionCharacterDamage damage;
        damage.handle = reader.i32(); damage.model = reader.i32(); damage.amount = reader.number();
        const uint32_t kind = reader.u32(); damage.kind = uint8_t(kind); damage.weapon = reader.i32();
        const uint32_t flags = reader.u32();
        if (kind > 255 || flags > 3) reader.ok = false;
        damage.headHit = (flags & 1) != 0; damage.critical = (flags & 2) != 0;
        damage.attackTime = reader.number();
        const uint32_t piece = reader.u32(), direction = reader.u32();
        if(piece > 6 || direction > 3) reader.ok = false;
        damage.hitPiece = uint8_t(piece); damage.hitDirection = uint8_t(direction);
        damage.playerAttack = true; damage.sourceKind = 1; damage.sourceHandle = 0;
        return damage;
    }
    void queueActor(const Actor& actor) {
        Writer writer(Type::Actor, epoch, ++sent); identity(writer, actor.identity);
        writer.number(actor.position.x); writer.number(actor.position.y); writer.number(actor.position.z);
        writer.number(actor.heading); writer.number(actor.health); writer.number(actor.armour); writer.number(actor.movementSpeed);
        writer.i32(actor.animationGroup); writer.i32(actor.animation); writer.i32(actor.weapon); writer.i32(actor.objective);
        writer.u32(actor.animationStartedAt); writer.u32(actor.deathStartedAt); writer.u32(actor.shotSerial);
        writer.u32((actor.alive ? 1u : 0u) | (actor.visible ? 2u : 0u) | (actor.animationLoop ? 4u : 0u) |
            (actor.walking ? 8u : 0u) | (actor.deathRecorded ? 16u : 0u));
        writer.u32(uint32_t(actor.kind)); writer.i32(actor.targetPlayer);
        writer.u32(actor.sampledAt); writer.number(actor.walkedDistance); writer.number(actor.walkSpeed);
        writer.i32(actor.vehicle); writer.i32(actor.passengerSeat); writer.u32(actor.movementState);
        writer.i32(actor.objectiveTarget); writer.i32(actor.lastWeaponDamage); writer.i32(actor.damageSourceHandle);
        writer.u32(actor.damageSourceKind); writer.i32(actor.ammunition); writer.i32(actor.magazine); writer.number(actor.reloading);
        writer.u32(actor.objectiveCompleted ? 1u : 0u);
        outgoing.push_back(std::move(writer.bytes));
    }
    static Actor readActor(Reader& reader, uint64_t epoch) {
        Actor actor; actor.identity = readIdentity(reader, epoch);
        actor.position.x = reader.number(); actor.position.y = reader.number(); actor.position.z = reader.number();
        actor.heading = reader.number(); actor.health = reader.number(); actor.armour = reader.number(); actor.movementSpeed = reader.number();
        actor.animationGroup = reader.i32(); actor.animation = reader.i32(); actor.weapon = reader.i32(); actor.objective = reader.i32();
        actor.animationStartedAt = reader.u32(); actor.deathStartedAt = reader.u32(); actor.shotSerial = reader.u32();
        const uint32_t flags = reader.u32(); if (flags > 31) reader.ok = false;
        actor.alive = (flags & 1) != 0; actor.visible = (flags & 2) != 0; actor.animationLoop = (flags & 4) != 0;
        actor.kind = Kind(reader.u32()); actor.targetPlayer = reader.i32(); actor.walking = (flags & 8) != 0;
        actor.deathRecorded = (flags & 16) != 0;
        actor.sampledAt = reader.u32(); actor.walkedDistance = reader.number(); actor.walkSpeed = reader.number();
        actor.vehicle = reader.i32(); actor.passengerSeat = reader.i32(); actor.movementState = reader.u32();
        actor.objectiveTarget = reader.i32(); actor.lastWeaponDamage = reader.i32(); actor.damageSourceHandle = reader.i32();
        actor.damageSourceKind = reader.u32(); actor.ammunition = reader.i32(); actor.magazine = reader.i32(); actor.reloading = reader.number();
        const uint32_t completed = reader.u32(); if (completed > 1) reader.ok = false; actor.objectiveCompleted = completed != 0;
        return actor;
    }
    bool room() const { return outgoing.size() < MaximumPending && sent != std::numeric_limits<uint64_t>::max(); }
    StorylandPlayTestRuntime& runtime;
    StorylandCoopAuthority authority;
    const bool host;
    bool completeScene = false;
    std::string cutsceneName;
    uint64_t cutsceneGeneration = 0;
    uint32_t cutsceneStartRequest = 0;
    bool cutscenePlaying = false, cutsceneSkipPending = false, cutsceneSkipApplied = false;
    uint64_t epoch = 0, sent = 0, received = 0;
    int32_t mission = -1;
    Phase phase = Phase::FreeRoam;
    std::string terminalKey;
    StorylandMissionFailure terminalFailure = StorylandMissionFailure::None;
    std::map<int32_t, Actor> replicas;
    std::deque<std::vector<uint8_t>> outgoing;
};
