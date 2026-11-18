#pragma once

#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include "src/re/reVCS/vehicles/storyland_vcs_hovercraft_water.h"
#include "src/re/reVCS/world/storyland_vcs_water_depth.h"

template<class SampleWater>
bool storylandApplyVcsObjectWater(StorylandMissionEnvironment::Object& object,
    const StorylandNativeCollisionModel& collision,const std::array<float,16>& matrix,
    float nativeStep,SampleWater sampleWater) {
    namespace Water=StorylandVcsHovercraft;
    if(!object.damage.propertiesLoaded||!std::isfinite(object.damage.turnMass)||
        object.damage.turnMass<=0||!Water::finite(object.velocity)||
        !Water::finite(object.rotationVelocity))return false;
    const Water::Frame frame{{matrix[0],matrix[1],matrix[2]},
        {matrix[4],matrix[5],matrix[6]},{matrix[8],matrix[9],matrix[10]}};
    Water::State state{Water::multiply(object.velocity,1.0f/50.0f),
        Water::multiply(object.rotationVelocity,1.0f/50.0f)};
    Water::WaterResult water;
    if(!Water::waterForces({collision.minimum.x,collision.minimum.y,collision.minimum.z},
        {collision.maximum.x,collision.maximum.y,collision.maximum.z},frame,
        {matrix[12],matrix[13],matrix[14]},state.velocity,object.damage.mass,
        object.damage.buoyancy,nativeStep,sampleWater,water))return false;
    if(!water.hasBuoyancy) {object.inWater=false;return true;}
    if(!object.damage.disableMoveForce)Water::moveForce(state,water.force,object.damage.mass);
    if(!object.damage.disableTurnForce)
        Water::turnForce(state,frame,{0,0,0},water.force,water.point,object.damage.turnMass);
    const float retention=std::pow(.9700000286102295f,nativeStep);
    state.velocity=Water::multiply(state.velocity,retention);
    state.angularVelocity=Water::multiply(state.angularVelocity,retention);
    if(!Water::finite(state.velocity)||!Water::finite(state.angularVelocity))return false;
    object.velocity=Water::multiply(state.velocity,50.0f);
    object.rotationVelocity=Water::multiply(state.angularVelocity,50.0f);
    object.inWater=water.hasBuoyancy;
    return true;
}
