// This is included after traceMissionVehicles and the script-object trace helper.
void storylandUpdateVcsVehicleSeaMines(StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& nativeWorld) {
    if(!runtime.crime.vcs||!runtime.missionEnvironment||runtime.coopGuest)return;
    auto& world=*runtime.missionEnvironment;
    if(!storylandVcsPadState(world,15)||!world.vehicleWorldMatrix)return;
    const auto weapon=std::find_if(runtime.weapons.begin(),runtime.weapons.end(),
        [](const auto& definition){return definition.nativeId==15;});
    if(weapon==runtime.weapons.end())return;
    for(const auto& entry:world.missionVehicles) {
        const int32_t handle=entry.first;
        const auto& scripted=entry.second;
        if(!scripted.vcsSeaMineLauncher||scripted.deleted)continue;
        if(scripted.spawnedSlot<0||size_t(scripted.spawnedSlot)>=runtime.vehicles.size())continue;
        const int slot=scripted.spawnedSlot;
        const auto& vehicle=runtime.vehicles[size_t(slot)];
        if(vehicle.scriptHandle!=handle||vehicle.nativeDefinition.domain!=StorylandVehicleDomain::Water)continue;
        const bool playerDriving=runtime.occupiedVehicle==slot&&runtime.playerVehicleSeat<0;
        const bool playerStatus=scripted.scriptStatus>=0?scripted.scriptStatus==0:playerDriving;
        if(!playerStatus||(!playerDriving&&!vehicle.hasDriver&&!vehicle.missionDriver&&scripted.driver<0))continue;
        std::array<float,16> matrix{};
        StorylandVcsVehicleSeaMineLaunch launch;
        if(!world.vehicleWorldMatrix(handle,matrix)||matrix[10]<-.6f||
            !storylandVcsVehicleSeaMineLaunch(matrix,launch))continue;
        float fraction=storylandTraceNativeSegmentCategories(nativeWorld,launch.lineStart,launch.lineEnd,true,false);
        StorylandNativePoint normal{};
        for(const auto& object:world.objects)
            storylandTraceScriptObject(nativeWorld,object,launch.lineStart,launch.lineEnd,fraction,normal);
        traceMissionVehicles(runtime,launch.lineStart,launch.lineEnd,fraction,normal,slot);
        if(fraction<1.0f)continue;
        const auto empty=std::find_if(world.scriptProjectiles.begin(),world.scriptProjectiles.end(),
            [](const auto& projectile){return !projectile.active;});
        if(empty==world.scriptProjectiles.end())continue;
        StorylandMissionEnvironment::ScriptProjectile projectile;
        projectile.active=true;projectile.weaponType=15;
        projectile.position={launch.position.x,launch.position.y,launch.position.z};
        projectile.velocity={launch.velocity.x,launch.velocity.y,launch.velocity.z};
        projectile.source={StorylandMissionEnvironment::DamageSourceKind::Vehicle,handle};
        projectile.yaw=launch.yaw;
        *empty=projectile;
    }
}
