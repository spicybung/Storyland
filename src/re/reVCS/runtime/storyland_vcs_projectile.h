#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_script_object.h"

inline bool storylandVcsProjectileGround(StorylandMissionEnvironment& world,
    std::array<float,3>& position,std::string& error) {
    if(!std::all_of(position.begin(),position.end(),[](float value){return std::isfinite(value);})) {
        error="Script projectile coordinates must be finite.";
        return false;
    }
    if(position[2]!=-100.0f&&position[2]>-250.0f)return true;
    float height=0;
    if(!world.groundHeightAt||!world.groundHeightAt({position[0],position[1],1000},height)||
        !std::isfinite(height)) {
        error="Script projectile ground sentinel requires native collision height.";
        return false;
    }
    position[2]=height;
    return true;
}

inline StorylandMissionEnvironment::DamageSource storylandVcsProjectileExplosionSource(
    const StorylandMissionEnvironment& world,const StorylandMissionEnvironment::ScriptProjectile& projectile) {
    auto source=projectile.source;
    if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Vehicle) {
        const auto vehicle=world.missionVehicles.find(source.handle);
        source={};
        if(vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted) {
            if(vehicle->second.driver>=0)
                source={StorylandMissionEnvironment::DamageSourceKind::Character,vehicle->second.driver};
            else if(world.inVehicle&&world.occupiedMissionVehicle==projectile.source.handle&&
                world.playerPassengerSeat<0)
                source={StorylandMissionEnvironment::DamageSourceKind::Character,0};
        }
    }
    return source;
}

inline bool storylandCreateVcsScriptProjectile(StorylandMissionEnvironment& world,
    int32_t weaponType,std::array<float,3> position,std::array<float,3> target,float power,
    StorylandMissionEnvironment::DamageSource source,int32_t flag,bool targeted,
    int32_t& handle,std::string& error) {
    if(!std::isfinite(power)||weaponType<12||weaponType>18||(targeted&&weaponType!=18)) {
        error="Native flying projectiles require rocket weapon 18; thrown projectiles require weapons 12 through 18.";
        return false;
    }
    if(!storylandVcsProjectileGround(world,position,error)||
        (targeted&&!storylandVcsProjectileGround(world,target,error)))return false;
    if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Vehicle) {
        const auto vehicle=world.missionVehicles.find(source.handle);
        if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)source={};
    } else if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Character&&
        source.handle!=0&&!world.missionCharacters.count(source.handle))source={};

    const auto vacant=std::find_if(world.scriptProjectiles.begin(),world.scriptProjectiles.end(),
        [](const auto& projectile){return !projectile.active;});
    if(vacant==world.scriptProjectiles.end()) {
        handle=0;
        return true;
    }
    StorylandMissionEnvironment::ScriptProjectile projectile;
    projectile.weaponType=weaponType;
    projectile.active=true;
    projectile.targeted=targeted;
    projectile.position=position;
    projectile.source=source;
    projectile.flag=flag;
    projectile.deadline=world.gameMilliseconds+(weaponType==16?20000u:
        weaponType==12||weaponType==13?2100u:2000u);
    if(weaponType==14||weaponType==15)projectile.deadline=0;

    float heading=0;
    std::array<float,3> inheritedVelocity{};
    if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Character) {
        if(source.handle==0)heading=world.heading;
        else {
            const auto& character=world.missionCharacters.at(source.handle);
            heading=character.heading;
            const float radians=heading*.01745329251994329577f;
            inheritedVelocity={-std::sin(radians)*character.movementSpeed,
                std::cos(radians)*character.movementSpeed,0};
        }
    }
    projectile.yaw=heading;
    const float radians=heading*.01745329251994329577f;
    if(targeted||weaponType==18) {
        std::array<float,3> direction{-std::sin(radians),std::cos(radians),0};
        if(targeted) {
            for(size_t axis=0;axis<3;++axis)direction[axis]=target[axis]-position[axis];
            const float length=std::hypot(direction[0],direction[1],direction[2]);
            if(length>0)for(float& component:direction)component/=length;
            else direction={1,0,0};
        }
        float speed=17.5f;
        if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Character)
            speed=(.35f*power+.15f)*50.0f;
        else if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Vehicle) {
            const auto& vehicle=world.missionVehicles.at(source.handle);
            if(vehicle.model==275) {
                std::array<float,16> matrix{};
                if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(source.handle,matrix)) {
                    error="Hunter rocket requires its live vehicle matrix.";
                    return false;
                }
                std::array<float,3> velocity=vehicle.velocity;
                if(world.vehicleWorldVelocity)world.vehicleWorldVelocity(source.handle,velocity);
                float right=0,forward=0;
                for(size_t axis=0;axis<3;++axis) {
                    right+=velocity[axis]*matrix[axis];
                    forward+=velocity[axis]*matrix[4+axis];
                }
                speed+=std::fabs(right)+std::fabs(forward);
                if(!targeted)direction={matrix[4],matrix[5],matrix[6]};
            }
        }
        for(size_t axis=0;axis<3;++axis)projectile.velocity[axis]=direction[axis]*speed;
        projectile.yaw=std::atan2(-direction[0],direction[1])*57.29577951308232f;
    } else if(weaponType!=14&&weaponType!=15) {
        const float speed=weaponType==17?std::max(.23f,.22f*power+.15f):
            power==0?0:.22f*power+.15f;
        projectile.velocity={-std::sin(radians)*speed*50,std::cos(radians)*speed*50,
            (weaponType==17?.2f*power+.4f:.4f*power+.4f)*speed*50};
        if(weaponType!=16)for(size_t axis=0;axis<3;++axis)
            projectile.velocity[axis]+=inheritedVelocity[axis];
    }
    if(!std::all_of(projectile.velocity.begin(),projectile.velocity.end(),
        [](float value){return std::isfinite(value);})) {
        error="Script projectile velocity is not finite.";
        return false;
    }
    handle=int32_t(vacant-world.scriptProjectiles.begin());
    *vacant=projectile;
    return true;
}

inline bool storylandRemoveVcsScriptProjectile(StorylandMissionEnvironment& world,
    int32_t handle,bool explode,std::string& error) {
    if(handle<0||size_t(handle)>=world.scriptProjectiles.size())return true;
    auto& projectile=world.scriptProjectiles[size_t(handle)];
    if(!projectile.active)return true;
    if(explode&&projectile.weaponType!=16) {
        const int32_t type=projectile.weaponType==18?2:projectile.weaponType==17?1:
            projectile.weaponType==14||projectile.weaponType==15?8:0;
        if(!storylandAddScriptExplosion(world,type,projectile.position,error,
            storylandVcsProjectileExplosionSource(world,projectile)))return false;
    }
    projectile.active=false;
    return true;
}

inline float storylandVcsRocketStep(float seconds,float& growth) {
    growth=std::pow(1.07f,seconds*30.0f);
    return std::expm1(std::log(1.07f)*seconds*30.0f)/(.07f*30.0f);
}
