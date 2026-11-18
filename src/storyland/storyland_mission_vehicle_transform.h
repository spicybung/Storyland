#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_vehicle_route.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include <cmath>

inline float storylandMissionVehicleFrameSuspension(const StorylandRuntimeVehicle& vehicle,
    const StorylandMissionEnvironment::VehicleScriptState& scripted) {
    const auto& definition=vehicle.nativeDefinition;
    if(scripted.teleportedByScript&&definition.vcsModelLayout&&definition.nativeType==4)return 0;
    if(scripted.teleportedByScript&&definition.vcsModelLayout&&
        (definition.nativeType==0||definition.nativeType==6)&&
        definition.hasNativeSuspension&&definition.collision)
        return -definition.collision->minimum.z-definition.nativeRideHeight;
    return vehicle.suspension;
}

inline StorylandVehicleMatrix storylandMissionVehicleWorldMatrix(const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,
    const StorylandMissionEnvironment::VehicleScriptState& scripted) {
    const auto& definition=vehicle.nativeDefinition;
    return storylandVehicleMatrixProduct(storylandVehicleMeshFrame(definition,
        scripted.movedByScript?std::array<float,3>{scripted.x,scripted.y,scripted.z}:
            std::array<float,3>{actor.position.x,actor.position.y,actor.position.z},
        scripted.headingChangedByScript?scripted.heading+90.0f:actor.yaw,
        definition.previewMinimumZ,storylandMissionVehicleFrameSuspension(vehicle,scripted),
        scripted.rotationChangedByScript?scripted.rotation[0]:vehicle.bodyRoll-vehicle.roadRoll,
        scripted.rotationChangedByScript?scripted.rotation[1]:vehicle.bodyPitch+vehicle.roadPitch,
        actor.water),definition.previewRoot);
}

inline void storylandApplyMissionVehicleTransform(StorylandRuntimeVehicle& vehicle,
    StorylandNativeTrafficActor& actor,StorylandMissionEnvironment::VehicleScriptState& scripted,
    const StorylandMissionEnvironment& world) {
    const bool teleported=scripted.teleportedByScript;
    if(teleported) {
        const int32_t nativeType=vehicle.nativeDefinition.nativeType;
        if(vehicle.nativeDefinition.vcsModelLayout&&
            (nativeType==0||nativeType==6||nativeType==1||nativeType==2||nativeType==4)) {
            // Teleport resets physical motion; later script vectors are already applied.
            vehicle.yawVelocity=0;
            vehicle.springRollVelocity=0;
            vehicle.springPitchVelocity=0;
            vehicle.suspensionVelocity=0;
            if(nativeType==0||nativeType==6) {
                // Road actors retain their ground-relative suspension representation.
                vehicle.suspension=storylandMissionVehicleFrameSuspension(vehicle,scripted);
            }
            if(nativeType==4) {
                vehicle.suspension=0;
                vehicle.bodyRoll-=vehicle.roadRoll;
                vehicle.bodyPitch+=vehicle.roadPitch;
                vehicle.roadRoll=vehicle.roadPitch=0;
            }
        }
        storylandInvalidateVehicleDestinationRoute(actor);
        vehicle.contactsValid=false;
        vehicle.wheelContacts={};
        vehicle.suspensionAnchorValid=false;
        vehicle.grounded=false;
        scripted.teleportedByScript=false;
    }
    if(scripted.movedByScript) {
        vehicle.pedestrianSweep={};
        actor.position={scripted.x,scripted.y,scripted.z};
        if(scripted.drivingToTarget) {
            actor.yaw=scripted.heading+90.0f;
            vehicle.speed=scripted.speed;
        }
    }
    if(scripted.headingChangedByScript)actor.yaw=scripted.heading+90.0f;
    if(scripted.velocityChangedByScript) {
        const float angle=actor.yaw*.0174532925199433f;
        vehicle.speed=scripted.velocity[0]*std::cos(angle)+scripted.velocity[1]*std::sin(angle);
        vehicle.lateralSpeed=-scripted.velocity[0]*std::sin(angle)+scripted.velocity[1]*std::cos(angle);
        vehicle.verticalSpeed=scripted.velocity[2];
        vehicle.grounded=false;
    }
    if(teleported&&vehicle.aiActive&&world.pedestrianPaths&&
        vehicle.nativeDefinition.domain==StorylandVehicleDomain::Road) {
        storylandJoinNativeVehicleRoute(*world.pedestrianPaths,actor,world.zones,
            vehicle.avoidLevelTransitions);
    }
}
