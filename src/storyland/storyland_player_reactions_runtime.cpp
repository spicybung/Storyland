#include "src/storyland/storyland_player_reactions.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float Radians = 0.01745329251994329577f;

StorylandPlayerReactionContext reactionContext(const StorylandPlayTestRuntime& runtime,
    const StorylandNativePoint& player, float heading, float seconds) {
    StorylandPlayerReactionContext result;
    result.position = player;
    result.heading = heading;
    result.health = runtime.health;
    result.movementSpeed = runtime.movementSpeed;
    result.seconds = seconds;
    result.vcs = runtime.crime.vcs;
    result.now = uint32_t(runtime.elapsed * 1000.0f);
    result.controlFrame = uint32_t(runtime.elapsed * StorylandVehicleStepsPerSecond);
    result.onFoot = runtime.occupiedVehicle < 0 &&
        runtime.vehicleAction == StorylandVehicleAction::None && runtime.entryTransition < 0;
    result.grounded = runtime.grounded;
    result.control = runtime.playerControl && !runtime.paused && !runtime.pauseMenu &&
        !runtime.busted && !runtime.wastedActive;
    result.water = runtime.playerWater.active;
    result.extendedMovement = runtime.extendedAndSaFeatures;
    result.crouched = runtime.extendedMovement.crouched;
    if (runtime.equippedWeapon < runtime.weapons.size()) {
        const auto& weapon = runtime.weapons[runtime.equippedWeapon];
        result.unarmed = weapon.nativeId == 0;
        result.meleeWeapon = weapon.fireType == 0;
        result.meleeReach = weapon.range;
    }
    if (runtime.missionEnvironment) {
        const auto& environment = *runtime.missionEnvironment;
        result.now = environment.gameMilliseconds;
        result.controlFrame = environment.nativeFrameCounter;
        result.control = result.control && environment.control;
        const bool attached = result.vcs ? environment.activeVcsCharacterAttachment(0) :
            environment.playerAttachedVehicle >= 0;
        result.onFoot = result.onFoot && !environment.playerPositionFrozen && !attached &&
            (result.vcs ? environment.remoteVehicle < 0 : environment.remoteControl.vehicle < 0);
        result.scripted = environment.cutscene.playing || environment.playerCutsceneSafe ||
            environment.mobilePhones.count(0) != 0 || environment.pedAnimationRequests.count(0) != 0 ||
            environment.scriptedCharacterAnimations.count(0) != 0 ||
            (!result.vcs && (environment.pedAnimationRequests.count(1) != 0 ||
                environment.scriptedCharacterAnimations.count(1) != 0));
    }
    return result;
}

StorylandReactionVehicle reactionVehicle(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors, size_t slot,bool forEvasion=false) {
    StorylandReactionVehicle result;
    if (slot >= actors.size() || slot >= runtime.vehicles.size()) return result;
    const auto& vehicle = runtime.vehicles[slot];
    const auto& actor = actors[slot];
    result.slot = int32_t(slot);
    result.model = vehicle.nativeDefinition.modelId;
    result.position = actor.position;
    result.heading = actor.yaw;
    result.halfLength = vehicle.halfLength;
    result.halfWidth = vehicle.halfWidth;
    result.height = vehicle.bodyHeight;
    const float angle = actor.yaw * Radians;
    result.velocity = {vehicle.speed * std::cos(angle) - vehicle.lateralSpeed * std::sin(angle),
        vehicle.speed * std::sin(angle) + vehicle.lateralSpeed * std::cos(angle), vehicle.verticalSpeed};
    result.active = vehicle.visible && vehicle.clumpVisible && vehicle.collisionEnabled &&
        !vehicle.craneCarried && !actor.water;
    if(forEvasion&&result.active) {
        bool driven=vehicle.hasDriver||vehicle.missionDriver||vehicle.coopControlled;
        if(!vehicle.coopControlled&&runtime.missionEnvironment&&vehicle.scriptHandle>=0) {
            const auto& environment=*runtime.missionEnvironment;
            const auto car=environment.missionVehicles.find(vehicle.scriptHandle);
            if(car!=environment.missionVehicles.end()&&car->second.driver>=2) {
                const auto driver=environment.missionCharacters.find(car->second.driver);
                if(driver!=environment.missionCharacters.end())
                    driven=driver->second.alive&&driver->second.health>0&&!driver->second.removeFromWorld;
            }
        }
        // Ignore stationary jitter and empty rolling vehicles for automatic
        // evasion only. Physical collision queries keep the original body.
        const float speedSquared=result.velocity.x*result.velocity.x+result.velocity.y*result.velocity.y;
        result.active=driven&&std::isfinite(speedSquared)&&speedSquared>=.25f;
    }
    return result;
}

StorylandReactionTarget ambientTarget(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& actors, int32_t slot) {
    StorylandReactionTarget result;
    if (slot < 0 || size_t(slot) >= actors.size() || size_t(slot) >= runtime.pedestrians.size())
        return result;
    const auto& ped = runtime.pedestrians[size_t(slot)];
    result.kind = StorylandReactionTargetKind::Ambient;
    result.handle = slot;
    result.model = ped.stats.modelId;
    result.position = actors[size_t(slot)].position;
    result.heading = actors[size_t(slot)].yaw;
    result.health = ped.health;
    result.armour = ped.armour;
    result.eligible = ped.scriptHandle < 0 && ped.health > 0 && !ped.water.active &&
        ped.state != StorylandPedState::Dead && !actors[size_t(slot)].vehicle && !actors[size_t(slot)].water;
    return result;
}

StorylandReactionTarget missionTarget(const StorylandPlayTestRuntime& runtime, int32_t handle) {
    StorylandReactionTarget result;
    if (!runtime.missionEnvironment || handle <= 0) return result;
    const auto& environment = *runtime.missionEnvironment;
    const auto found = environment.missionCharacters.find(handle);
    if (found == environment.missionCharacters.end()) return result;
    const auto& character = found->second;
    result.kind = StorylandReactionTargetKind::Mission;
    result.handle = handle;
    result.model = uint32_t(character.model);
    result.position = {character.x, character.y, character.z};
    result.heading = character.heading + 90;
    result.health = character.health;
    result.armour = character.armour;
    const auto combat = runtime.missionCombat.find(handle);
    const bool water = combat != runtime.missionCombat.end() && combat->second.water.active;
    result.eligible = character.alive && character.health > 0 && character.visible &&
        character.vehicle < 0 && !character.attached && !character.positionFrozen &&
        !character.neverTargeted && !character.playerFriend && !character.networkControlled &&
        (!runtime.crime.vcs || character.vcsScriptFlags.acceptsPairedActions) &&
        !character.removeFromWorld && !water && character.animationId < 0 &&
        environment.pedAnimationRequests.count(handle) == 0 &&
        environment.scriptedCharacterAnimations.count(handle) == 0;
    return result;
}

bool canReachTarget(const StorylandNativeWorld& world, const StorylandPlayerReactionContext& context,
    const StorylandReactionTarget& target) {
    if (!target.eligible || context.meleeReach <= 0) return false;
    const float dx = target.position.x - context.position.x;
    const float dy = target.position.y - context.position.y;
    if (std::hypot(dx, dy) > context.meleeReach || std::fabs(target.position.z - context.position.z) >= 1)
        return false;
    const float bearing = std::atan2(dy, dx) / Radians;
    if (std::fabs(std::remainder(bearing - context.heading, 360.0f)) > 60) return false;
    auto from = context.position, to = target.position;
    from.z += 1.1f;
    to.z += 1.1f;
    return storylandTraceNativeSegment(world, from, to) >= 1;
}

StorylandReactionTarget selectedTarget(const StorylandPlayTestRuntime& runtime,
    const StorylandPlayerReactionState& state, const StorylandNativeWorld& world,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandPlayerReactionContext& context, bool targetHeld) {
    if (state.holding()) {
        auto target = state.target.kind == StorylandReactionTargetKind::Ambient ?
            ambientTarget(runtime, pedestrians, state.target.handle) : missionTarget(runtime, state.target.handle);
        const float distance = std::hypot(target.position.x - context.position.x,
            target.position.y - context.position.y);
        if (distance > std::max(context.meleeReach, state.gripDistance) ||
            std::fabs(target.position.z - context.position.z) >= 1) target.eligible = false;
        return target;
    }
    if (!targetHeld || !context.unarmed) return {};
    if (runtime.targetedPed >= 0) return ambientTarget(runtime, pedestrians, runtime.targetedPed);
    if (runtime.missionEnvironment) {
        const auto& target = runtime.missionEnvironment->playerTarget;
        if (target.kind == StorylandMissionEnvironment::TargetKind::Character)
            return missionTarget(runtime, target.handle);
        if (target.kind != StorylandMissionEnvironment::TargetKind::None) return {};
    }
    StorylandReactionTarget closest;
    float best = std::numeric_limits<float>::infinity();
    const auto consider = [&](const StorylandReactionTarget& candidate) {
        if (!canReachTarget(world, context, candidate)) return;
        const float distance = std::hypot(candidate.position.x - context.position.x,
            candidate.position.y - context.position.y);
        if (distance < best) { best = distance; closest = candidate; }
    };
    for (size_t slot = 0; slot < pedestrians.size() && slot < runtime.pedestrians.size(); ++slot)
        consider(ambientTarget(runtime, pedestrians, int32_t(slot)));
    if (runtime.missionEnvironment)
        for (const auto& entry : runtime.missionEnvironment->missionCharacters)
            consider(missionTarget(runtime, entry.first));
    return closest;
}

void applyGrabContact(StorylandPlayTestRuntime& runtime, const StorylandReactionTarget& target,
    bool fatal, const StorylandPlayerReactionPose& terminalPose) {
    if (runtime.equippedWeapon >= runtime.weapons.size()) return;
    const auto& weapon = runtime.weapons[runtime.equippedWeapon];
    const auto rememberFatalPose = [&](StorylandPlayerReactionPose& pose) {
        pose = terminalPose;
        pose.repeat = false;
        runtime.statistics["NecksSnapped"] += 1;
    };
    const float strength = runtime.playerStats.valid ? runtime.playerStats.attackStrength : 1;
    float damage = weapon.damage * strength;
    if (!std::isfinite(damage) || damage <= 0) return;
    if (target.kind == StorylandReactionTargetKind::Ambient && target.handle >= 0 &&
        size_t(target.handle) < runtime.pedestrians.size()) {
        const auto& ped = runtime.pedestrians[size_t(target.handle)];
        if (ped.stats.modelId != target.model) return;
        if (fatal) {
            const float weakness = ped.stats.valid ? std::clamp(ped.stats.defendWeakness, 0.35f, 3.0f) : 1;
            damage = std::max(damage, (ped.health + ped.armour) / weakness);
        }
        storylandDamagePlayTestPed(runtime, size_t(target.handle), damage, target.position, true);
        auto& damaged = runtime.pedestrians[size_t(target.handle)];
        if (fatal && target.health > 0 && damaged.health <= 0)
            rememberFatalPose(damaged.pairedDeathPose);
    } else if (target.kind == StorylandReactionTargetKind::Mission && runtime.missionEnvironment) {
        const auto found = runtime.missionEnvironment->missionCharacters.find(target.handle);
        if (found == runtime.missionEnvironment->missionCharacters.end() ||
            uint32_t(found->second.model) != target.model) return;
        const auto& character = found->second;
        const bool critical = fatal && character.suffersCriticalHits && character.criticalHits;
        if (critical) {
            const float weakness = character.pedStat >= 0 ? character.behavior.statDefendWeakness : 1;
            if (std::isfinite(weakness) && weakness > 0)
                damage = std::max(damage, (character.health + character.armour) / weakness);
        }
        StorylandMissionCharacterDamage request;
        request.handle = target.handle;
        request.model = int32_t(target.model);
        request.amount = damage;
        request.playerAttack = true;
        request.kind = 16;
        request.weapon = weapon.nativeId;
        request.sourceKind = uint8_t(StorylandMissionEnvironment::DamageSourceKind::Character);
        request.sourceHandle = 0;
        request.critical = critical;
        request.attackTime = runtime.elapsed;
        if (!runtime.routeMissionCharacterDamage || !runtime.routeMissionCharacterDamage(request))
            storylandApplyMissionCharacterDamage(runtime, request);
        const auto damaged = runtime.missionEnvironment->missionCharacters.find(target.handle);
        if (fatal && target.health > 0 && damaged != runtime.missionEnvironment->missionCharacters.end() &&
            damaged->second.health <= 0)
            rememberFatalPose(runtime.missionCombat[target.handle].pairedDeathPose);
        runtime.crime.report(StorylandCrime::Assault, target.position, runtime.elapsed,
            character.type == 6, uint32_t(target.handle));
    }
    StorylandPlayTestSoundEvent sound(StorylandPlayTestSound::Melee);
    sound.position = target.position;
    sound.positioned = true;
    sound.weapon = int(runtime.equippedWeapon);
    runtime.soundEvents.push_back(sound);
}
}

StorylandReactionVehicle storylandPlayerReactionVehicle(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& vehicles, size_t slot) {
    return reactionVehicle(runtime, vehicles, slot);
}

void storylandNotifyPlayerVehicleAnnoyance(StorylandPlayTestRuntime& runtime,
    StorylandPlayerReactionState& state, const std::vector<StorylandNativeTrafficActor>& vehicles,
    size_t slot, uint32_t serial, bool policeEvent) {
    auto context = reactionContext(runtime, runtime.playerPosition, 0, 1.0f / StorylandVehicleStepsPerSecond);
    context.firing = runtime.playerMelee.active || runtime.elapsed == runtime.playerLastShotTime;
    if (!state.temperInitialized) {
        state.temper = runtime.playerStats.valid ? runtime.playerStats.temper : 50;
        state.temperInitialized = true;
    }
    storylandNotifyPlayerVehicleAnnoyance(state, context, reactionVehicle(runtime, vehicles, slot), serial, policeEvent);
}

bool storylandRequestScriptedPlayerEvasiveDive(StorylandPlayTestRuntime& runtime,
    int32_t vehicle, std::string& error) {
    if (!runtime.missionEnvironment) {
        error = "The player evasive dive requires the live mission world.";
        return false;
    }
    auto& world = *runtime.missionEnvironment;
    const auto found = world.missionVehicles.find(vehicle);
    if (found == world.missionVehicles.end() || found->second.deleted) {
        error = "Evasive dive requires an existing vehicle.";
        return false;
    }
    // 41C128 forces SetEvasiveDive; 1FD9E0 tests physical ped control,
    // independently of the script's disabled player input or camera.
    if (!(world.health > 0) || !(runtime.health > 0) || runtime.wastedActive || runtime.busted ||
        runtime.occupiedVehicle >= 0 || world.inVehicle || runtime.entryTransition >= 0 ||
        runtime.vehicleAction != StorylandVehicleAction::None || world.activeVcsCharacterAttachment(0) ||
        runtime.vehicleExitPose.active || runtime.playerJetSkiFallActive ||
        !runtime.grounded || runtime.jumpLaunching || runtime.landingTime > 0 ||
        runtime.playerReaction.action == StorylandPlayerReaction::EvadeDive) return true;
    std::array<float,3> velocity = found->second.velocity;
    if (world.vehicleWorldVelocity && !world.vehicleWorldVelocity(vehicle, velocity)) {
        error = "The evasive target vehicle has no live velocity.";
        return false;
    }
    if (!std::isfinite(velocity[0]) || !std::isfinite(velocity[1])) {
        error = "The evasive target velocity is non-finite.";
        return false;
    }
    if (!runtime.reactionBindings.duration || !runtime.reactionBindings.rootDelta ||
        !(runtime.reactionBindings.duration(146, 0) > 0)) {
        error = "The native player EV_dive animation and root track are unavailable.";
        return false;
    }
    const uint8_t direction = world.playerVcsScriptFlags.evasiveDirection;
    const uint32_t random = direction == 0 ? world.nextNativeRandom() : 0;
    const float heading = storylandVcsForcedEvasionHeading(velocity[0], velocity[1], direction, random);
    if (!storylandBeginScriptedPlayerEvasiveDive(runtime.playerReaction, heading, runtime.reactionBindings)) {
        error = "The native player evasive dive could not start.";
        return false;
    }
    runtime.playerReaction.playerHealth = world.health;
    world.playerWalkTask.clear();
    world.heading = heading - 90;
    storylandClearPlayTestTarget(runtime);
    error.clear();
    return true;
}

bool storylandUpdatePlayerCombatReactions(StorylandPlayTestRuntime& runtime,
    StorylandPlayerReactionState& state, const StorylandPlayerReactionBindings& bindings,
    const StorylandNativeWorld& world, std::vector<StorylandNativeTrafficActor>& pedestrians,
    const std::vector<StorylandNativeTrafficActor>& vehicles, StorylandNativePoint& player,
    float& heading, bool& jump, bool& interact, bool targetHeld, bool& fire, bool& reload,
    bool forward, float seconds) {
    if(storylandPlayerBulletReactionActive(runtime)) {
        state.clearAction();runtime.playerMelee={};
        runtime.jumpButtonHeld=jump;
        jump=false;interact=false;fire=false;reload=false;
        runtime.movementSpeed=0;runtime.playerMoveBlend=0;
        return true;
    }
    const auto context = reactionContext(runtime, player, heading, seconds);
    auto target = selectedTarget(runtime, state, world, pedestrians, context,
        targetHeld || runtime.pcGrabHeld);
    if (!state.holding() && target.eligible && !canReachTarget(world, context, target)) target.eligible = false;
    std::vector<StorylandReactionVehicle> candidates;
    candidates.reserve(std::min(vehicles.size(), runtime.vehicles.size()));
    for (size_t slot = 0; slot < vehicles.size() && slot < runtime.vehicles.size(); ++slot)
        candidates.push_back(reactionVehicle(runtime, vehicles, slot,true));
    const StorylandPlayerReactionInput input{jump, runtime.pcCombatControls ? false : interact,
        fire, targetHeld, forward, runtime.pcGrabHeld,
        runtime.pcCancelAim || (runtime.pcCombatControls && interact)};
    const auto result = storylandUpdatePlayerReaction(state, context, input,
        target.kind == StorylandReactionTargetKind::None ? nullptr : &target, candidates, bindings);
    if (result.consumeJump) { jump = false; runtime.jumpButtonHeld = input.jump; }
    if (result.consumeInteract) interact = false;
    if (result.consumeFire) { fire = false; reload = false; runtime.playerMelee = {}; }
    if (result.releasedTarget) storylandClearPlayTestTarget(runtime);
    if (!result.ownsMovement) return false;
    if (state.holding()) {
        runtime.playerAiming = true;
        runtime.targetVisible = true;
        runtime.targetLockRequested = true;
        runtime.targetWeapon = runtime.equippedWeapon;
        runtime.targetedPed = target.kind == StorylandReactionTargetKind::Ambient ? target.handle : -1;
        runtime.targetPosition = target.position;
        runtime.targetPosition.z += 1.0f;
        if (runtime.missionEnvironment) {
            auto& selected = runtime.missionEnvironment->playerTarget;
            selected = {};
            if (target.kind == StorylandReactionTargetKind::Mission) {
                selected.kind = StorylandMissionEnvironment::TargetKind::Character;
                selected.handle = target.handle;
            }
        }
        float maximumHealth = 100.0f;
        if (target.kind == StorylandReactionTargetKind::Mission && runtime.missionEnvironment) {
            const auto character = runtime.missionEnvironment->missionCharacters.find(target.handle);
            if (character != runtime.missionEnvironment->missionCharacters.end())
                maximumHealth = std::max(1.0f, character->second.maximumHealth);
        }
        runtime.targetIndicator.update(runtime.targetedPed,
            uint8_t(target.kind == StorylandReactionTargetKind::Mission ?
                StorylandMissionEnvironment::TargetKind::Character : StorylandMissionEnvironment::TargetKind::None),
            target.handle, target.health / maximumHealth, context.controlFrame);
    }
    runtime.movementSpeed = 0;
    runtime.playerMoveBlend = 0;
    runtime.playerSprintBlend = 0;
    runtime.playerSprintActive = false;
    if (state.holding()) {
        runtime.extendedMovement.cancel();
        runtime.extendedMovement.crouched = false;
        runtime.extendedMovement.crouchStanceBlend = 0;
        runtime.extendedMovement.crouchMoveBlend = 0;
        runtime.playerCrouchPose = {};
    }
    storylandResetPlayerSprint(runtime);
    runtime.playerStopVariant = -1;
    runtime.playerStopTime = 0;
    runtime.jumpLaunching = false;
    heading = state.heading;
    auto next = player;
    next.x += result.displacement.x;
    next.y += result.displacement.y;
    const int ignoredPed = target.kind == StorylandReactionTargetKind::Ambient ? target.handle : -1;
    const auto movement = storylandMovePlayTestCharacter(world, runtime, player, next, ignoredPed);
    if (target.kind != StorylandReactionTargetKind::None && !result.releasedTarget) {
        auto nextVictim = target.position;
        nextVictim.x += result.victimDisplacement.x;
        nextVictim.y += result.victimDisplacement.y;
        const auto victimMovement = storylandMovePlayTestCharacter(world, runtime,
            target.position, nextVictim, ignoredPed);
        if (!victimMovement.clear || victimMovement.blocked || !movement.clear || movement.blocked) {
            state.clearAction();
            storylandClearPlayTestTarget(runtime);
            return true;
        }
        player = movement.position;
        const float victimHeading = state.heading + (state.rearGrip() ? 0 : 180);
        if (target.kind == StorylandReactionTargetKind::Ambient && target.handle >= 0 &&
            size_t(target.handle) < pedestrians.size()) {
            auto& actor = pedestrians[size_t(target.handle)];
            actor.position = victimMovement.position;
            actor.yaw = victimHeading;
            if (size_t(target.handle) < runtime.movementPedestrians.size())
                runtime.movementPedestrians[size_t(target.handle)] = actor.position;
            auto& ped = runtime.pedestrians[size_t(target.handle)];
            ped.melee = {};
            ped.ranged = {};
            ped.combatSpeed = 0;
        } else if (target.kind == StorylandReactionTargetKind::Mission && runtime.missionEnvironment) {
            auto& character = runtime.missionEnvironment->missionCharacters.at(target.handle);
            character.x = victimMovement.position.x;
            character.y = victimMovement.position.y;
            character.z = victimMovement.position.z;
            character.heading = victimHeading - 90;
            character.movementSpeed = 0;
            auto& combat = runtime.missionCombat[target.handle];
            combat.melee = {};
            combat.nativeRootValid = false;
        }
    } else if (movement.clear) player = movement.position;
    runtime.playerPosition = player;
    if (result.damageContact) {
        auto terminalPose = result.victimPose;
        if (result.fatalContact) { terminalPose.animation = 311; terminalPose.group = 63; }
        if (bindings.duration)
            terminalPose.seconds = std::max(0.0f, bindings.duration(terminalPose.animation, terminalPose.group) - 0.001f);
        applyGrabContact(runtime, target, result.fatalContact, terminalPose);
    }
    return true;
}

void storylandUpdatePlayerCombatControls(StorylandPlayTestRuntime& runtime,
    const StorylandPlayTestInput& input, bool aiming) {
    const int32_t weapon = runtime.equippedWeapon < runtime.weapons.size() ?
        runtime.weapons[runtime.equippedWeapon].nativeId : -1;
    const bool available = runtime.playerControl && runtime.health > 0 && !runtime.wastedActive &&
        !runtime.busted && !runtime.paused && !runtime.pauseMenu && !runtime.playerWater.active &&
        runtime.occupiedVehicle < 0 && runtime.vehicleAction == StorylandVehicleAction::None &&
        (!runtime.missionEnvironment || (runtime.missionEnvironment->control &&
            !runtime.missionEnvironment->cutscene.playing && !runtime.missionEnvironment->camera.active));
    runtime.pcCombatControls = input.pcControls;
    runtime.pcGrabHeld = input.grab && available && runtime.crime.vcs && weapon == 0;
    runtime.pcCancelAim = input.cancelAim;
    const bool pressed = input.scopeToggle && !runtime.equalizerScopeHeld;
    runtime.equalizerScopeHeld = input.scopeToggle;
    if (weapon != runtime.combatControlWeapon || !available || !runtime.crime.vcs || weapon != 31)
        runtime.equalizerScope = false;
    runtime.combatControlWeapon = weapon;
    if (available && input.pcControls && runtime.crime.vcs && weapon == 31 && aiming && pressed)
        runtime.equalizerScope = !runtime.equalizerScope;
    if (input.cancelAim) runtime.equalizerScope = false;
    if ((!runtime.equalizerScope || !aiming) && runtime.manualAimMode == 7 && weapon == 31 &&
        (!runtime.missionEnvironment || !runtime.missionEnvironment->manualAim.forced)) {
        runtime.manualAimActive = false;
        runtime.manualAimMode = 0;
    }
}

int32_t storylandPlayerWeaponManualAimMode(const StorylandPlayTestRuntime& runtime,
    int32_t weapon) {
    if (runtime.pcCombatControls && runtime.crime.vcs && weapon == 31 && !runtime.equalizerScope) return 0;
    return storylandWeaponManualAimMode(weapon);
}

bool storylandEqualizerUsesNormalAim(const StorylandPlayTestRuntime& runtime) {
    return runtime.pcCombatControls && runtime.crime.vcs && !runtime.equalizerScope &&
        runtime.equippedWeapon < runtime.weapons.size() &&
        runtime.weapons[runtime.equippedWeapon].nativeId == 31;
}

bool storylandPlayerUsesBareHands(const StorylandPlayTestRuntime& runtime) {
    return runtime.equippedWeapon < runtime.weapons.size() &&
        runtime.equippedWeapon < runtime.inventory.size() &&
        runtime.inventory[runtime.equippedWeapon].owned &&
        runtime.weapons[runtime.equippedWeapon].nativeId == 0;
}

StorylandPlayerReactionPose storylandHeldVictimPose(const StorylandPlayTestRuntime& runtime,
    StorylandReactionTargetKind kind, int32_t handle, uint32_t model,
    const StorylandPlayerReactionPose& terminalPose) {
    if (storylandPlayerReactionHoldsTarget(runtime.playerReaction, kind, handle, model))
        return storylandPlayerReactionPose(runtime.playerReaction, true);
    return terminalPose;
}
