#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include <cmath>

// Simulation time, independent of visible fire particles and their lifetime.
inline bool storylandVehicleExplosionDue(StorylandRuntimeVehicle& vehicle,bool boat,float seconds) {
    if(!std::isfinite(seconds)||seconds<=0)return false;
    const bool ignitionDamage=vehicle.health<250.0f;
    if(!vehicle.wrecked&&!ignitionDamage) {
        vehicle.charred=false;vehicle.engineFireTime=0;vehicle.engineFireSpent=false;
    }
    if(vehicle.charred||!vehicle.canBeDamaged||(vehicle.water.immersion>.1f&&!boat)) {
        vehicle.engineFireTime=0;return false;
    }
    if(vehicle.health<=0)return true;
    if(ignitionDamage&&!vehicle.engineFireSpent&&!(vehicle.damageProofs&2u))
        vehicle.engineFireTime+=seconds;
    else vehicle.engineFireTime=0;
    return vehicle.engineFireTime>=5.0f;
}
