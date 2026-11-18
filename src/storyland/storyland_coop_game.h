#pragma once
#include "src/storyland/storyland_time.h"
#include "src/storyland/storyland_coop_sync.h"
#include "src/storyland/storyland_mission_program.h"
#include "src/storyland/storyland_multiplayer.h"
#include "src/storyland/storyland_coop_vehicles.h"
#include "src/storyland/storyland_vehicle_systems.h"
#include "src/storyland/storyland_vehicle_entry.h"
#include "src/storyland/storyland_script_object.h"
#include "src/storyland/storyland_ped_damage.h"
#include "src/storyland/storyland_character_sound_events.h"
#include <set>

class StorylandCoopGame {
public:
    StorylandCoopGame(StorylandPlayTestRuntime& playRuntime, StorylandMissionVm& missionVm,
        StorylandMissionProgram& missionProgram, StorylandMultiplayer& connection)
        : runtime(playRuntime), vm(missionVm), program(missionProgram), network(connection), environment(*playRuntime.missionEnvironment), sync(playRuntime, connection.isHost()) {
        runtime.coopConnected = true;
        runtime.coopPlayerIndex = network.isHost() ? 0 : 1;
        runtime.coopFriendlyFire = network.friendlyFire();
        wasDead = runtime.health <= 0 || runtime.busted;
        runtime.coopHitPlayer = [this](const StorylandCoopPlayerHit& hit) {
            if (!network.friendlyFire()) return;
            if(!sync.hitPlayer(hit))status="Player hit could not be sent: the connection is not ready.";
        };
        sync.onPlayerHit = [this](const StorylandCoopPlayerHit& hit) {
            if (!network.friendlyFire()) return true;
            if (hit.kind == 4) return receiveExplosion(hit);
            if(!collisionWorld || hit.targetLife!=runtime.coopLife || runtime.health<=0 || runtime.busted ||
                !runtime.coopRemoteAlive || (lastPlayerShot && int32_t(hit.shot-lastPlayerShot)<=0))return false;
            const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
                [&](const auto& w){return w.nativeId==hit.weapon;});
            if(weapon==runtime.weapons.end() || !std::isfinite(weapon->damage) || weapon->damage<=0 ||
                (hit.kind==1 && weapon->fireType!=1) || (hit.kind==16 && weapon->fireType!=0))return false;
            const auto distance=[](const StorylandNativePoint& a,const StorylandNativePoint& b) {
                return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
            };
            const auto attacker=playerTorso(network.remote);
            const auto target=playerTorso(localPlayer);
            // Allow movement during one snapshot interval, but never accept distant
            // hit origins or a claimed ray through static geometry.
            if(distance(hit.origin,attacker)>3 || distance(hit.impact,target)>2 ||
                distance(hit.origin,hit.impact)>weapon->range+1 ||
                storylandTraceNativeSegment(*collisionWorld,hit.origin,hit.impact)<.995f ||
                (lastPlayerAttack>=0 && hit.attackTime-lastPlayerAttack<std::max(.01f,weapon->interval)*.8f))return false;
            lastPlayerShot=hit.shot; lastPlayerAttack=hit.attackTime;
            storylandRequestWeaponHitSpeech(runtime,-1,runtime.playerPosition,hit.weapon);
            if (runtime.inflictDamage(weapon->damage, uint8_t(hit.kind), hit.weapon, true,
                hit.hitPiece, hit.hitDirection, true)) {
                world().playerLastWeaponDamage=hit.weapon;
                world().playerDamageSource={StorylandMissionEnvironment::DamageSourceKind::Character,-2};
            }
            return true;
        };
        sync.onParticles = [this](float sampledAt,const std::vector<StorylandRuntimeParticle>& particles) {
            const float age=std::max(0.0f,network.remote.elapsed+float(std::clamp(clock-remoteSampleTime,0.0,1.0))-sampledAt);
            for(auto p:particles) {
                if(runtime.particles.size()>=2048)break;
                if(p.remaining<=age)continue;
                p.remaining-=age;
                storylandAdvanceNativeParticlePresentation(p,age);
                if(p.remaining<=0)continue;
                p.position.x+=p.velocity.x*age; p.position.y+=p.velocity.y*age; p.position.z+=p.velocity.z*age;
                runtime.particles.push_back(p);
            }
        };
        sync.onVehicleEvent = [this](const auto& event) { return receiveVehicleEvent(event); };
        runtime.coopVehicleEntryAllowed = [this](int32_t slot, int32_t seat, const StorylandNativePoint& position) {
            return permitVehicleEntry(slot, seat, position);
        };
        runtime.coopPreferredVehicleSeat = [this](int32_t slot, const StorylandNativePoint& position) {
            if (!vehicleActors || slot < 0 || size_t(slot) >= vehicleActors->size() ||
                size_t(slot) >= runtime.vehicles.size() || !runtime.vehicles[size_t(slot)].coopControlled) return -1;
            return storylandCoopAtDriverDoor(position, (*vehicleActors)[size_t(slot)], runtime.vehicles[size_t(slot)]) ? -1 : 0;
        };
        runtime.coopDisplaceDriver = [this](uint32_t identity, const StorylandNativePoint& position) {
            if (!identity || identity != approvedJack || pendingJackLife != runtime.coopLife) return false;
            StorylandCoopSync::VehicleEvent event;
            event.action = StorylandCoopSync::VehicleEvent::Action::Take;
            event.identity = identity; event.life = runtime.coopLife; event.model = pendingJackModel; event.position = position;
            if (!sync.vehicleEvent(event)) return false;
            takingVehicle = identity;
            approvedJack = pendingJack = 0; pendingJackSlot = -1;
            return true;
        };
        lastResult = world().missionResult.sequence;
        runtime.coopGuest = world().coopGuest = !network.isHost();
        sync.onMissionBegin = [this](int32_t index, bool restarted) {
            resetActorBindings(); requestedCutsceneSkip.clear();
            appliedResultEpoch = reportedResultEpoch = 0; stoppedByResult = false;
            if (restarted) {
                if (!restartMission || !restartMission(index))
                    status = "The host restarted the mission, but the local restart could not be queued.";
            } else if (index >= 0 && index < 0x40000000 && !program.startCoopMission(index, vm))
                status = "The host's mission could not be started in the local script program.";
            lastResult = world().missionResult.sequence;
        };
        sync.onMissionResult = [this](uint64_t epoch, int32_t, StorylandCoopSync::Phase phase,
            const std::string& key, StorylandMissionFailure failure) {
            if (appliedResultEpoch == epoch) return;
            appliedResultEpoch = epoch;
            const auto outcome = phase == StorylandCoopSync::Phase::Passed ?
                StorylandMissionOutcome::Passed : StorylandMissionOutcome::Failed;
            const bool localResult = (world().missionResult.sequence != lastResult || reportedResultEpoch == epoch) &&
                world().missionResult.outcome == outcome && world().missionResult.key == key;
            if (!localResult) {
                program.applyCoopOutcome(vm, outcome, failure, key);
                stoppedByResult = true;
            }
            lastResult = world().missionResult.sequence;
            runtime.statistics = world().statistics;
        };
        runtime.routeMissionCharacterDamage = [this](const StorylandMissionCharacterDamage& request) {
            if (!runtime.coopGuest) return false;
            auto timed = request;
            timed.attackTime = runtime.elapsed;
            if (timed.playerAttack && !sync.requestHit(timed.handle, timed))
                status = "Co-op hit could not be sent: actor is not ready or the send queue is full.";
            return true;
        };
        if (network.isHost()) {
            world().coopTargetPosition = [this](int32_t handle, std::array<float,3>& point) {
                const auto found = world().missionCharacters.find(handle);
                if (found == world().missionCharacters.end()) return -1;
                const auto& character = found->second;
                const bool explicitAttack = character.objective == 0xE4 || character.objective == 0xE5;
                const bool police = (character.type == 6 || character.pedStats.name == "STAT_COP") &&
                    !explicitAttack && !character.provokedByPlayer;
                const int player = sync.npcTarget({character.x, character.y, character.z}, police);
                point = player == 1 ? std::array<float,3>{network.remote.x,network.remote.y,network.remote.z} :
                    std::array<float,3>{world().x,world().y,world().z};
                return player;
            };
            runtime.coopChooseTarget = [this](const StorylandNativePoint& position, bool police) {
                return sync.npcTarget(position, police);
            };
            runtime.coopDamageRemote = [this](float amount, int32_t weapon, uint8_t kind, int32_t attacker) {
                if (!sync.damageRemotePlayer(amount, weapon, kind, attacker)) status = "Co-op damage send queue is full.";
            };
        }
    }
    ~StorylandCoopGame() {
        runtime.coopHitPlayer = {}; runtime.coopRemoteLife = 0;
        runtime.coopConnected = false; runtime.coopRemoteVehicle = -1; runtime.coopPlayerIndex = 0;
        runtime.coopVehicleEntryAllowed = {}; runtime.coopPreferredVehicleSeat = {}; runtime.coopDisplaceDriver = {};
        for (auto& vehicle : runtime.vehicles) { vehicle.coopControlled = false; vehicle.coopSeatReserved = false; vehicle.coopIdentity = 0; }
        runtime.routeMissionCharacterDamage = {};
        runtime.coopChooseTarget = {}; runtime.coopDamageRemote = {};
        world().coopTargetPosition = {};
        runtime.coopGuest = world().coopGuest = runtime.coopRemoteAlive = false;
        for (auto& entry : world().missionCharacters) entry.second.networkControlled = false;
    }
    StorylandCoopGame(const StorylandCoopGame&) = delete;
    StorylandCoopGame& operator=(const StorylandCoopGame&) = delete;
    std::string status;
    std::function<void(int32_t)> actorRemoved;
    std::function<void(int32_t, float, float)> actorMovement;
    StorylandCoopSync::Validator validateHit;
    const StorylandNativeWorld* collisionWorld = nullptr;
    std::vector<StorylandNativeTrafficActor>* vehicleActors = nullptr;
    std::function<bool(int32_t)> ensureVehicleModel;
    std::function<bool()> skipCutscene;
    // Queue a platform restart; do not destroy this connection inside its receive callback.
    std::function<bool(int32_t)> restartMission;

    void restartWorld() {
        restartRequested = network.isHost(); cursor = 0; nextPublish = 0;
        requestedCutsceneSkip.clear(); lastPlayerAttack = -1; lastPlayerShot = 0;
        lastExplosionShot = 0;
        pendingJack = approvedJack = grantedJack = takingVehicle = 0;
        pendingJackSlot = -1; sentGarages.clear();
        for (auto& vehicle : runtime.vehicles) vehicle.coopSeatReserved = false;
        appliedResultEpoch = reportedResultEpoch = 0; stoppedByResult = false;
        lastResult = world().missionResult.sequence;
        if (++runtime.coopLife == 0) ++runtime.coopLife;
        wasDead = runtime.health <= 0 || runtime.busted;
    }
    bool requestCutsceneSkip() {
        if (!network.connected() || !world().cutscene.playing || world().cutscene.name.empty()) return false;
        requestedCutsceneSkip = world().cutscene.name;
        return true;
    }
    bool applyingCutsceneSkip() const { return applyingSkip; }

    void beforeSimulation(const StorylandNetworkPlayer& local, double now) {
        clock = now; localPlayer=local;
        runtime.coopFriendlyFire = network.friendlyFire();
        world().coopFriendlyFire = runtime.coopFriendlyFire;
        if (network.isHost()) {
            world().coopGuestStage = storylandCoopGuestStage(vm.vcs, world().passedMissions,
                world().missionText, world().coopGuestStage);
            network.setGuestStage(world().coopGuestStage);
        } else {
            world().coopGuestStage = network.guestStage();
            storylandSetPlayTestClock(runtime,network.remote.gameHour);
            world().hour=runtime.clockHour;
            world().clockResetRequested=false;
            if (world().forcedWeather != network.remote.weather) {
                world().forcedWeather = network.remote.weather; world().weatherChanged = true;
            }
        }
        prepareVehicles();
        const bool dead=runtime.health<=0 || runtime.busted;
        if(wasDead && !dead) { if(++runtime.coopLife==0)++runtime.coopLife; }
        wasDead=dead;
        if(remoteLife!=network.remote.life) {remoteLife=network.remote.life;lastPlayerAttack=-1;lastPlayerShot=lastExplosionShot=0;}
        if(lastRemoteSequence!=network.remoteSequence()) {
            remoteSampleTime=now;lastRemoteSequence=network.remoteSequence();
            if(remoteShotKnown && network.remote.shotSerial!=remoteShot && network.remote.health>0) {
                const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
                    [&](const auto& w){return w.nativeId==network.remote.weapon;});
                if(weapon!=runtime.weapons.end() && runtime.soundEvents.size()<256) {
                    StorylandPlayTestSoundEvent event(weapon->fireType==0?StorylandPlayTestSound::MeleeSwing:StorylandPlayTestSound::WeaponShot);
                    event.position={network.remote.x,network.remote.y,network.remote.z+1};event.positioned=true;
                    event.weapon=int(weapon-runtime.weapons.begin());event.speaker=-2;runtime.soundEvents.push_back(event);
                }
            }
            remoteShot=network.remote.shotSerial;remoteShotKnown=true;
        }
        runtime.coopRemoteLife=network.remote.life;
        setPlayer(network.isHost() ? 0 : 1, local);
        setPlayer(network.isHost() ? 1 : 0, network.remote);
        runtime.coopRemotePosition = {network.remote.x, network.remote.y, network.remote.z};
        runtime.coopRemoteTorso = playerTorso(network.remote);
        runtime.coopRemoteHeading = network.remote.heading;
        runtime.coopRemoteAlive = network.connected() && network.remote.health > 0 &&
            !network.remote.dead() && !network.remote.busted() && network.remote.visible();
        if (restartRequested) return;
        if (network.isHost()) sync.observeCutscene(world().cutscene.name, world().cutscene.playing,
            world().cutscene.startRequest);
        const auto result = sync.pump(network, validateHit);
        if (result.rejected) status = "Co-op ignored an invalid or outdated gameplay message.";
        if (!requestedCutsceneSkip.empty()) {
            if (!world().cutscene.playing || requestedCutsceneSkip != world().cutscene.name ||
                sync.requestCutsceneSkip(requestedCutsceneSkip)) requestedCutsceneSkip.clear();
        }
        if (skipCutscene && sync.takeCutsceneSkip(world().cutscene.name, world().cutscene.playing)) {
            struct SkipGuard {
                bool& flag;
                explicit SkipGuard(bool& flag) : flag(flag) { flag = true; }
                ~SkipGuard() { flag = false; }
            } guard(applyingSkip);
            if (!skipCutscene()) status = "The synchronized cutscene skip could not be applied.";
        }
        applyActors();
        if (approvedJack && pendingJackSlot >= 0 && vehicleActors && runtime.occupiedVehicle < 0 &&
            runtime.vehicleAction == StorylandVehicleAction::None && pendingJackLife == runtime.coopLife) {
            if (!storylandBeginPlayTestVehicleEntry(runtime, *vehicleActors, runtime.playerPosition, pendingJackSlot, -1)) {
                approvedJack = pendingJack = 0; pendingJackSlot = -1;
            }
        }
    }

    void afterSimulation(int32_t activeMission, double now) {
        prepareVehicles();
        publishGarages();
        const auto result = world().missionResult;
        if (!restartRequested && sync.missionId() && sync.missionPhase() == StorylandCoopSync::Phase::Running &&
            result.sequence != lastResult && result.outcome != StorylandMissionOutcome::None) {
            const auto phase = result.outcome == StorylandMissionOutcome::Passed ?
                StorylandCoopSync::Phase::Passed : StorylandCoopSync::Phase::Failed;
            const std::string key = result.key.substr(0, 64);
            const bool sent = network.isHost() ? sync.setMissionPhase(phase, key, result.failure) :
                sync.requestMissionPhase(phase, key, result.failure);
            if (sent) { lastResult = result.sequence; reportedResultEpoch = sync.missionId(); }
        }
        // A terminal event may finish the active VM in the callback above.
        if (stoppedByResult) activeMission = -1;
        if (network.isHost()) {
            bool epochReady = true;
            if (restartRequested || !sync.missionId() || sync.missionIndex() != activeMission) {
                if (sync.beginMission(activeMission, activeMission < 0 ?
                    StorylandCoopSync::Phase::FreeRoam : StorylandCoopSync::Phase::Running, restartRequested)) {
                    cursor = 0; nextPublish = 0; lastResult = world().missionResult.sequence; stoppedByResult = false;
                    restartRequested = false;
                } else epochReady = false;
            }
            if (epochReady) sync.observeCutscene(world().cutscene.name, world().cutscene.playing,
                world().cutscene.startRequest);
            if (epochReady && now >= nextPublish && network.pendingGameMessages() < 16 && sync.pending() < 8) {
                nextPublish = now + 0.1;
                std::vector<int32_t> removed;
                for (const auto& entry : sync.actors())
                    if (!world().missionCharacters.count(entry.first)) removed.push_back(entry.first);
                for (int32_t handle : removed) if (!sync.removeActor(handle)) break;
                unsigned count = 0;
                auto actor = world().missionCharacters.upper_bound(cursor);
                for (; actor != world().missionCharacters.end() && count < 8; ++actor, ++count) {
                    const auto& character = actor->second;
                    const bool police = character.type == 6 || character.pedStats.name == "STAT_COP";
                    const bool enemy = character.provokedByPlayer || (character.fearFlags & 1u) ||
                        ((character.objective == 0xE4 || character.objective == 0xE5) && character.objectiveTarget == 0);
                    if (!sync.publishActor(actor->first, police ? StorylandCoopSync::Kind::Police :
                        enemy ? StorylandCoopSync::Kind::Enemy : StorylandCoopSync::Kind::Civilian)) break;
                    cursor = actor->first;
                }
                if (actor == world().missionCharacters.end() && sync.completeSnapshot()) cursor = 0;
            }
        } else applyActors();
        // Send short-lived presentation once, in bounded batches. Dropping excess
        // smoke under congestion must not delay damage or mission results.
        const auto urgentParticle=[](const StorylandRuntimeParticle& p) {
            return p.kind==StorylandParticleKind::MuzzleFlash||p.kind==StorylandParticleKind::Explosion||
                p.kind==StorylandParticleKind::Flash||p.kind==StorylandParticleKind::Blood;
        };
        const bool urgent=std::any_of(runtime.particles.begin(),runtime.particles.end(),
            [&](const auto& p){return !p.coopSent&&urgentParticle(p);});
        if(now>=nextParticles || urgent) {
            nextParticles=now+.05;
            std::vector<StorylandRuntimeParticle> batch;
            std::vector<size_t> selected;
            const bool canSend=network.pendingGameMessages()<8 && sync.pending()<8;
            for(bool important:{true,false})for(size_t index=0;index<runtime.particles.size();++index) {
                auto& p=runtime.particles[index];
                if(p.coopSent||urgentParticle(p)!=important)continue;
                if(p.remaining<=0 || p.lifetime>60 || p.size<=0 ||
                    std::hypot(p.position.x-localPlayer.x,p.position.y-localPlayer.y,p.position.z-localPlayer.z)>80) {
                    p.coopSent=true;continue;
                }
                if(!canSend || batch.size()==8) {
                    if(!important)p.coopSent=true;
                    continue;
                }
                batch.push_back(p);selected.push_back(index);
            }
            if(!batch.empty() && sync.publishParticles(runtime.elapsed,batch))
                for(size_t index:selected)runtime.particles[index].coopSent=true;
        }
        if (restartRequested) sync.flush([&](const auto& bytes){return network.sendGameMessage(bytes);});
        else sync.pump(network, validateHit);
    }

    const StorylandCoopSync& state() const { return sync; }
private:
    bool receiveExplosion(const StorylandCoopPlayerHit& hit) {
        StorylandExplosionProfile profile;
        if (hit.targetLife != runtime.coopLife || runtime.health <= 0 || runtime.busted ||
            network.remote.area != world().area ||
            (lastExplosionShot && int32_t(hit.shot-lastExplosionShot) <= 0) ||
            !storylandExplosionProfile(hit.explosionType, profile)) return false;
        if (!vm.vcs) profile.weapon = storylandNativeExplosionWeapon(false);
        if (hit.weapon != profile.weapon || std::fabs(hit.attackTime-network.remote.elapsed) > 3 ||
            std::hypot(hit.impact.x-world().x,hit.impact.y-world().y,hit.impact.z-world().z) > 3) return false;
        lastExplosionShot = hit.shot;
        const float distance = std::hypot(hit.origin.x-world().x,hit.origin.y-world().y,hit.origin.z-world().z);
        const float factor = distance < profile.radius ? std::min(1.0f, 2.0f*(1-distance/profile.radius)) : 0;
        if (factor <= 0) return true;
        if (runtime.occupiedVehicle >= 0 && runtime.vehicleAction == StorylandVehicleAction::None) {
            if (size_t(runtime.occupiedVehicle) >= runtime.vehicles.size()) return false;
            auto& car = runtime.vehicles[size_t(runtime.occupiedVehicle)];
            if (!car.coopControlled && car.canBeDamaged && !(car.damageProofs & 4u)) {
                car.health = std::max(0.0f, car.health-1100*factor); car.visiblyDamaged = true;
                const auto scripted = world().missionVehicles.find(car.scriptHandle);
                if (scripted != world().missionVehicles.end()) {
                    scripted->second.health = car.health; scripted->second.alive = car.health > 0;
                    scripted->second.lastWeaponDamage = profile.weapon;
                    scripted->second.lastDamageSource = {StorylandMissionEnvironment::DamageSourceKind::Character, -2};
                }
            }
        } else if (runtime.inflictDamage(250*factor, 4, profile.weapon, true, 0, 0, true)) {
            world().playerLastWeaponDamage = profile.weapon;
            world().playerDamageSource = {StorylandMissionEnvironment::DamageSourceKind::Character, -2};
        }
        return true;
    }
    bool permitVehicleEntry(int32_t slot, int32_t seat, const StorylandNativePoint& position) {
        if (!vehicleActors || slot < 0 || size_t(slot) >= vehicleActors->size() || size_t(slot) >= runtime.vehicles.size()) return true;
        const auto& vehicle = runtime.vehicles[size_t(slot)];
        if (seat >= 0 || !vehicle.coopControlled) return true;
        if (!storylandCoopAtDriverDoor(position, (*vehicleActors)[size_t(slot)], vehicle)) return false;
        if (approvedJack == vehicle.coopIdentity && pendingJackLife == runtime.coopLife) return true;
        if (pendingJack == vehicle.coopIdentity && pendingJackLife == runtime.coopLife && clock < jackDeadline) return false;
        StorylandCoopSync::VehicleEvent event;
        event.identity = vehicle.coopIdentity; event.life = runtime.coopLife;
        event.model = int32_t(vehicle.nativeDefinition.modelId); event.position = position;
        if (sync.vehicleEvent(event)) {
            pendingJack = event.identity; pendingJackLife = event.life; pendingJackModel = event.model;
            pendingJackSlot = slot; jackDeadline = clock + 3;
        }
        return false;
    }
    bool receiveVehicleEvent(const StorylandCoopSync::VehicleEvent& event) {
        using Action = StorylandCoopSync::VehicleEvent::Action;
        if (!vehicleActors) return false;
        if (event.action == Action::Grant || event.action == Action::Reject) {
            if (event.identity != pendingJack || event.life != pendingJackLife ||
                event.model != pendingJackModel || event.life != runtime.coopLife || clock > jackDeadline) return false;
            if (event.action == Action::Grant) { approvedJack = event.identity; jackDeadline = clock + 10; }
            else { pendingJack = approvedJack = 0; pendingJackSlot = -1; }
            return true;
        }
        if (event.life != network.remote.life) return false;
        int32_t slot = storylandCoopVehicleSlot(runtime, *vehicleActors, event.identity, event.model);
        if (event.action == Action::Garage) {
            if (slot < 0) return false;
            auto& vehicle = runtime.vehicles[size_t(slot)];
            if (vehicle.garageOwner != uint8_t(1-runtime.coopPlayerIndex) || slot == runtime.occupiedVehicle ||
                slot == runtime.transitionVehicle) return false;
            if (event.garage >= 0) {
                const auto garage = std::find_if(world().garages.begin(), world().garages.end(),
                    [&](const auto& value) { return value.handle == event.garage; });
                if (garage == world().garages.end() ||
                    !storylandGarageContainsPoint(*garage, {event.position.x,event.position.y,event.position.z}, 1)) return false;
                vehicle.savedGarage = event.garage; vehicle.visible = false; vehicle.collisionEnabled = false;
                vehicle.positionFrozen = true; vehicle.coopControlled = false;
                (*vehicleActors)[size_t(slot)].position = event.position;
            } else if (vehicle.savedGarage >= 0) {
                vehicle.savedGarage = -1; vehicle.visible = true; vehicle.collisionEnabled = true;
                vehicle.positionFrozen = false; vehicle.parked = true;
            }
            return true;
        }
        if (slot < 0 || slot != runtime.occupiedVehicle || runtime.playerVehicleSeat >= 0 ||
            runtime.health <= 0 || runtime.busted || world().playerStayInCarWhenJacked) return false;
        const auto& actor = (*vehicleActors)[size_t(slot)];
        auto& vehicle = runtime.vehicles[size_t(slot)];
        if (event.action == Action::Request) {
            const bool valid = runtime.vehicleAction == StorylandVehicleAction::None &&
                network.remote.health > 0 && !network.remote.dead() && !network.remote.busted() &&
                network.remote.area == world().area &&
                std::hypot(network.remote.x-event.position.x,network.remote.y-event.position.y,
                    network.remote.z-event.position.z) < 2.5f &&
                storylandCoopAtDriverDoor(event.position, actor, vehicle) && std::fabs(vehicle.speed) < 2.0f &&
                (!collisionWorld || storylandTraceNativeSegment(*collisionWorld,
                    {event.position.x,event.position.y,event.position.z+1},
                    {actor.position.x,actor.position.y,actor.position.z+1}) >= .995f);
            auto response = event; response.action = valid ? Action::Grant : Action::Reject;
            if (!sync.vehicleEvent(response)) return false;
            if (valid) {
                grantedJack = event.identity; grantedJackLife = event.life; grantedDeadline = clock + 10;
                vehicle.coopSeatReserved = true; vehicle.speed = vehicle.lateralSpeed = 0;
            }
            return true;
        }
        if (event.action != Action::Take || grantedJack != event.identity ||
            grantedJackLife != event.life || clock > grantedDeadline ||
            !storylandCoopAtDriverDoor(event.position, actor, vehicle)) return false;
        grantedJack = 0;
        vehicle.coopSeatReserved = false;
        runtime.occupiedVehicle = -1; runtime.playerVehicleSeat = -1;
        runtime.vehicleAction = StorylandVehicleAction::None; runtime.transitionVehicle = -1;
        runtime.vehicleTransitionPrepared = false;
        runtime.playerPosition = event.position;
        runtime.movementSpeed = 0; world().speed = 0;
        world().inVehicle = false; world().occupiedMissionVehicle = -1; world().playerPassengerSeat = -1;
        world().playerVehicleTransition = false; world().playerWarped = true;
        world().x = event.position.x; world().y = event.position.y; world().z = event.position.z;
        vehicle.garageOwner = uint8_t(1-runtime.coopPlayerIndex); vehicle.hasDriver = false;
        return true;
    }
    void prepareVehicles() {
        if (!vehicleActors) return;
        if (grantedJack && (clock > grantedDeadline || grantedJackLife != network.remote.life)) {
            for (auto& vehicle : runtime.vehicles) if (vehicle.coopIdentity == grantedJack) vehicle.coopSeatReserved = false;
            grantedJack = 0;
        }
        const int32_t localSlot = runtime.vehicleAction == StorylandVehicleAction::None ?
            runtime.occupiedVehicle : runtime.transitionVehicle;
        if (localSlot >= 0 && size_t(localSlot) < runtime.vehicles.size() && size_t(localSlot) < vehicleActors->size()) {
            auto& vehicle = runtime.vehicles[size_t(localSlot)];
            if (!vehicle.coopIdentity) {
                if (vehicle.scriptHandle >= 0 && vehicle.scriptHandle < 0x40000000)
                    vehicle.coopIdentity = 0x40000000u | uint32_t(vehicle.scriptHandle + 1);
                else {
                    if (++nextVehicleIdentity >= 0x40000000u) nextVehicleIdentity = 1;
                    vehicle.coopIdentity = nextVehicleIdentity | (runtime.coopPlayerIndex ? 0x80000000u : 0u);
                }
            }
            if (runtime.occupiedVehicle == localSlot && runtime.playerVehicleSeat < 0 && !vehicle.coopControlled)
                vehicle.garageOwner = runtime.coopPlayerIndex;
        }
        const int32_t previous = runtime.coopRemoteVehicle;
        const int32_t current = network.remote.vehicleModel < 0 ? -1 :
            storylandBindCoopVehicle(runtime, *vehicleActors, network.remote, ensureVehicleModel);
        if (previous >= 0 && previous != current && size_t(previous) < runtime.vehicles.size()) {
            auto& vehicle = runtime.vehicles[size_t(previous)];
            vehicle.coopControlled = false; vehicle.hasDriver = false; vehicle.parked = true;
        }
        runtime.coopRemoteVehicle = current;
        if (takingVehicle && (localSlot != current || network.remote.vehicleIdentity != takingVehicle ||
            !network.remote.seated() || network.remote.seat >= 0)) takingVehicle = 0;
        if (current >= 0) {
            auto& vehicle = runtime.vehicles[size_t(current)];
            const bool takingDriverSeat = takingVehicle && vehicle.coopIdentity == takingVehicle &&
                localSlot == current && runtime.playerVehicleSeat < 0;
            if (takingDriverSeat || !network.remote.seated() || network.remote.seat >= 0)
                vehicle.coopControlled = false;
            else storylandApplyCoopVehicle(runtime, *vehicleActors, current, network.remote);
        }
        if (pendingJack && (pendingJackLife != runtime.coopLife || clock > jackDeadline)) {
            pendingJack = approvedJack = 0; pendingJackSlot = -1;
        }
    }
    void publishGarages() {
        if (!vehicleActors || !sync.missionId()) return;
        for (size_t slot = 0; slot < runtime.vehicles.size() && slot < vehicleActors->size(); ++slot) {
            auto& vehicle = runtime.vehicles[slot];
            if (!vehicle.coopIdentity || vehicle.garageOwner != runtime.coopPlayerIndex) continue;
            const int32_t garage = vehicle.visible ? -1 : vehicle.savedGarage;
            const auto previous = sentGarages.find(vehicle.coopIdentity);
            if (previous == sentGarages.end() && garage < 0) { sentGarages[vehicle.coopIdentity] = -1; continue; }
            if (previous != sentGarages.end() && previous->second == garage) continue;
            StorylandCoopSync::VehicleEvent event;
            event.action = StorylandCoopSync::VehicleEvent::Action::Garage;
            event.identity = vehicle.coopIdentity; event.life = runtime.coopLife;
            event.model = int32_t(vehicle.nativeDefinition.modelId); event.garage = garage;
            event.position = (*vehicleActors)[slot].position;
            if (sync.vehicleEvent(event)) sentGarages[vehicle.coopIdentity] = garage;
            else break;
        }
    }
    void resetActorBindings() {
        for (const auto& binding : bindings) {
            const auto found = world().missionCharacters.find(binding.first);
            if (found != world().missionCharacters.end() && found->second.networkControlled) {
                storylandDeleteMissionCharacter(world(), binding.first);
                runtime.missionCombat.erase(binding.first);
            }
            if (actorRemoved) actorRemoved(binding.first);
        }
        bindings.clear(); lastAppliedAt = 0;
    }
    static StorylandNativePoint playerTorso(const StorylandNetworkPlayer& player) {
        return (player.actionFlags&32)?StorylandNativePoint{player.torsoX,player.torsoY,player.torsoZ}:
            StorylandNativePoint{player.x,player.y,player.z+1};
    }
    StorylandMissionEnvironment& world() { return environment; }
    void setPlayer(unsigned index, const StorylandNetworkPlayer& source) {
        StorylandCoopAuthority::Player player;
        player.connected = network.connected(); player.alive = source.health > 0 && !source.dead();
        player.arrested = source.busted(); player.policeIgnore = source.policeIgnore(); player.wanted = source.wanted;
        player.position = {source.x, source.y, source.z}; sync.setPlayer(index, player);
    }
    void applyActors() {
        if (network.isHost() || !sync.sceneReady()) return;
        std::vector<int32_t> removed;
        for (const auto& binding : bindings)
            if (!sync.actors().count(binding.first)) removed.push_back(binding.first);
        for (int32_t handle : removed) {
            storylandDeleteMissionCharacter(world(), handle);
            runtime.missionCombat.erase(handle); bindings.erase(handle);
            if (actorRemoved) actorRemoved(handle);
        }
        const float seconds = float(clock - lastAppliedAt);
        const float blend = float(std::clamp((clock - lastAppliedAt) * 14.0, 0.0, 1.0));
        lastAppliedAt = clock;
        for (const auto& entry : sync.actors()) {
            const auto& state = entry.second;
            const auto previous = bindings.find(entry.first);
            if (previous != bindings.end() && previous->second.generation != state.identity.generation) {
                storylandDeleteMissionCharacter(world(), entry.first); runtime.missionCombat.erase(entry.first);
                if (actorRemoved) actorRemoved(entry.first);
            }
            auto& binding = bindings[entry.first];
            const bool newActor = binding.generation != state.identity.generation;
            if (newActor || binding.sampledAt != state.sampledAt) {
                binding.receivedAt = clock; binding.sampledAt = state.sampledAt;
            }
            binding.generation = state.identity.generation;
            const auto local = world().missionCharacters.find(entry.first);
            const bool changedModel = local != world().missionCharacters.end() &&
                local->second.model != state.identity.model;
            if (changedModel) {
                storylandDeleteMissionCharacter(world(), entry.first); runtime.missionCombat.erase(entry.first);
                if (actorRemoved) actorRemoved(entry.first);
            }
            auto& character = world().missionCharacters[entry.first];
            character.networkControlled = true; character.model = state.identity.model;
            const float previousX = character.x, previousY = character.y;
            const bool snapped = newActor || std::hypot(character.x-state.position.x,
                character.y-state.position.y,character.z-state.position.z)>25;
            const float weight = snapped ? 1.0f : blend;
            character.x += (state.position.x-character.x)*weight;
            character.y += (state.position.y-character.y)*weight;
            character.z += (state.position.z-character.z)*weight;
            character.heading += std::remainder(state.heading-character.heading,360.0f)*weight; character.health = state.health; character.armour = state.armour;
            character.alive = state.alive; character.visible = state.visible;
            character.deathRecorded = state.deathRecorded;
            character.animationGroup = state.animationGroup; character.animationId = state.animation;
            character.animationLoop = state.animationLoop;
            const uint32_t presentationElapsed = uint32_t(std::clamp(clock-binding.receivedAt,0.0,0.15)*1000.0);
            character.animationStartedAt = world().gameMilliseconds - (state.sampledAt - state.animationStartedAt) - presentationElapsed;
            character.deathStartedAt = world().gameMilliseconds - (state.sampledAt - state.deathStartedAt) - presentationElapsed;
            character.currentWeapon = state.weapon; character.objective = state.objective;
            character.objectiveTarget = state.objectiveTarget; character.objectiveCompleted = state.objectiveCompleted;
            character.lastWeaponDamage = state.lastWeaponDamage;
            character.lastDamageSource = {StorylandMissionEnvironment::DamageSourceKind(state.damageSourceKind),state.damageSourceHandle};
            auto& inventory = character.weapons[state.weapon]; inventory.owned = true; inventory.ammunition = state.ammunition;
            character.walkingToTarget = state.walking; character.movementSpeed = state.movementSpeed;
            character.walkedDistance += (state.walkedDistance-character.walkedDistance)*weight; character.walkSpeed = state.walkSpeed;
            character.movementState = uint8_t(state.movementState);
            character.vehicle = state.vehicle; character.passengerSeat = state.passengerSeat;
            if (actorMovement && std::isfinite(seconds) && seconds > 0) {
                float forward = 0, right = 0;
                if (!snapped && !changedModel && character.alive && character.health > 0 &&
                    character.vehicle < 0 && !character.attached && !character.positionFrozen &&
                    !character.stayInSamePlace && character.movementSpeed > 0) {
                    const float velocityX = (character.x - previousX) / seconds;
                    const float velocityY = (character.y - previousY) / seconds;
                    if (std::isfinite(velocityX) && std::isfinite(velocityY)) {
                        const float facing = (character.heading + 90) * .01745329251994329577f;
                        forward = velocityX * std::cos(facing) + velocityY * std::sin(facing);
                        right = velocityX * std::sin(facing) - velocityY * std::cos(facing);
                    }
                }
                actorMovement(entry.first, forward, right);
            }
            auto& combat = runtime.missionCombat[entry.first];
            combat.shotSerial = state.shotSerial; combat.nativeWeapon = state.weapon;
            combat.magazine = state.magazine; combat.reloading = state.reloading;
            bool knownWeapon = false;
            for (size_t weapon = 0; weapon < runtime.weapons.size(); ++weapon)
                if (runtime.weapons[weapon].nativeId == state.weapon) { combat.weapon = weapon; knownWeapon = true; break; }
            if (!newActor && state.shotSerial != binding.shotSerial && knownWeapon) {
                character.hasShot = true; character.shotAt = world().gameMilliseconds;
                const bool firearm = runtime.weapons[combat.weapon].fireType != 0;
                const auto muzzle = combat.muzzleValid ? combat.muzzle :
                    StorylandNativePoint{character.x,character.y,character.z+1.0f};
                if (runtime.soundEvents.size() < 256) {
                    StorylandPlayTestSoundEvent event(firearm ? StorylandPlayTestSound::WeaponShot : StorylandPlayTestSound::MeleeSwing);
                    event.position = muzzle; event.positioned = true; event.weapon = int(combat.weapon);
                    event.speaker = int(runtime.pedestrians.size()) + entry.first;
                    runtime.soundEvents.push_back(event);
                }
                // Native muzzle/impact particles arrive through the effect stream.
            }
            binding.shotSerial = state.shotSerial;

        }
    }
    StorylandNetworkPlayer localPlayer;
    uint32_t nextVehicleIdentity = 0;
    uint32_t pendingJack = 0, approvedJack = 0, pendingJackLife = 0;
    int32_t pendingJackSlot = -1, pendingJackModel = -1;
    uint32_t grantedJack = 0, grantedJackLife = 0, takingVehicle = 0;
    double jackDeadline = 0, grantedDeadline = 0;
    std::map<uint32_t,int32_t> sentGarages;
    bool wasDead=false,remoteShotKnown=false;
    uint32_t lastPlayerShot=0,lastExplosionShot=0,remoteShot=0,remoteLife=0,lastRemoteSequence=0;
    float lastPlayerAttack=-1;
    double nextParticles=0,remoteSampleTime=0;
    StorylandPlayTestRuntime& runtime;
    StorylandMissionVm& vm;
    StorylandMissionProgram& program;
    StorylandMultiplayer& network;
    StorylandMissionEnvironment& environment;
    StorylandCoopSync sync;
    struct Binding { uint64_t generation = 0; uint32_t sampledAt = 0, shotSerial = 0; double receivedAt = 0; };
    std::map<int32_t, Binding> bindings;
    uint64_t lastResult = 0, appliedResultEpoch = 0, reportedResultEpoch = 0;
    int32_t cursor = 0;
    double nextPublish = 0, clock = 0, lastAppliedAt = 0;
    bool stoppedByResult = false, restartRequested = false, applyingSkip = false;
    std::string requestedCutsceneSkip;
};
