#pragma once
#include "src/storyland/storyland_script_object.h"
#include "src/re/reVCS/world/storyland_vcs_rope_object.h"
#include "src/re/reVCS/runtime/storyland_vcs_mission_garage.h"

enum class StorylandBetaVehicleBomb : uint8_t { Timed=1, Ignition=2 };

inline int32_t storylandVehicleBombDriver(const StorylandMissionEnvironment& world,
    int32_t handle,const StorylandMissionEnvironment::VehicleScriptState& vehicle) {
    return world.inVehicle&&world.occupiedMissionVehicle==handle&&world.playerPassengerSeat<0?
        0:vehicle.driver;
}

inline void storylandClearVehicleBomb(StorylandMissionEnvironment::VehicleScriptState& vehicle) {
    vehicle.bombType=0;vehicle.bombOwner=-1;vehicle.bombDeadline=0;
    vehicle.previousBombDriver=-1;vehicle.bombCountdownActive=false;vehicle.betaBombInstalled=false;
}

inline void storylandInstallVehicleBomb(const StorylandMissionEnvironment& world,int32_t handle,
    StorylandMissionEnvironment::VehicleScriptState& vehicle,int32_t type) {
    storylandClearVehicleBomb(vehicle);
    vehicle.bombType=uint8_t(type)&7;
    if(vehicle.bombType) {
        vehicle.bombOwner=0;
        vehicle.previousBombDriver=storylandVehicleBombDriver(world,handle,vehicle);
    }
}

inline bool storylandArmVehicleBomb(const StorylandMissionEnvironment& world,int32_t handle,
    StorylandMissionEnvironment::VehicleScriptState& vehicle) {
    if(vehicle.deleted||vehicle.wrecked||world.remoteVehicle>=0||
        storylandVehicleBombDriver(world,handle,vehicle)!=0)return false;
    if(vehicle.bombType==1) {
        // Retained timed-bomb arming establishes a seven-second game-clock fuse.
        vehicle.bombType=4;vehicle.bombDeadline=world.gameMilliseconds+7000;
        vehicle.bombCountdownActive=true;
    } else if(vehicle.bombType==2) {
        vehicle.bombType=5;vehicle.bombDeadline=0;vehicle.bombCountdownActive=false;
        vehicle.previousBombDriver=0;
    } else return false;
    return true;
}

inline bool storylandAdvanceVehicleBomb(const StorylandMissionEnvironment& world,int32_t handle,
    StorylandMissionEnvironment::VehicleScriptState& vehicle,bool armPressed) {
    if(vehicle.deleted||vehicle.wrecked)return false;
    const int32_t driver=storylandVehicleBombDriver(world,handle,vehicle);
    if(armPressed)storylandArmVehicleBomb(world,handle,vehicle);
    if(vehicle.bombType==4&&!vehicle.bombCountdownActive) {
        // Scripts can install an already armed timed bomb directly.
        vehicle.bombDeadline=world.gameMilliseconds+7000;vehicle.bombCountdownActive=true;
    } else if(vehicle.bombType==5&&!vehicle.bombCountdownActive&&
        driver>=0&&vehicle.previousBombDriver<0) {
        // Retained ignition behavior starts a one-second fuse when a driver enters.
        // Once started, leaving or re-entering the vehicle cannot reset it.
        vehicle.bombDeadline=world.gameMilliseconds+1000;vehicle.bombCountdownActive=true;
    }
    vehicle.previousBombDriver=driver;
    return (vehicle.bombType==4||vehicle.bombType==5)&&vehicle.bombCountdownActive&&
        int32_t(world.gameMilliseconds-vehicle.bombDeadline)>=0;
}

inline uint32_t storylandVehicleBombMillisecondsRemaining(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::VehicleScriptState& vehicle) {
    if(!vehicle.bombCountdownActive)return 0;
    const int32_t remaining=int32_t(vehicle.bombDeadline-world.gameMilliseconds);
    return remaining>0?uint32_t(remaining):0;
}

inline bool storylandFitBetaVehicleBomb(StorylandMissionEnvironment& world,int32_t handle,
    StorylandBetaVehicleBomb mode,bool vcs,std::string& error) {
    error.clear();
    if(!vcs) {error="These bomb service variants require Vice City Stories.";return false;}
    if(mode!=StorylandBetaVehicleBomb::Timed&&mode!=StorylandBetaVehicleBomb::Ignition) {
        error="Select a timed or ignition bomb.";return false;
    }
    const auto found=world.missionVehicles.find(handle);
    if(found==world.missionVehicles.end()||found->second.deleted||found->second.wrecked||
        !found->second.alive||found->second.health<=0) {
        error="Enter a working car before fitting a bomb.";return false;
    }
    auto& vehicle=found->second;
    if(!world.inVehicle||world.occupiedMissionVehicle!=handle||world.playerPassengerSeat>=0) {
        error="You must be driving the car.";return false;
    }
    if(!world.vehicleNativeType||world.vehicleNativeType(handle)!=0) {
        error="This service fits native automobiles only.";return false;
    }
    const auto contacts=world.vehicleContacts?world.vehicleContacts(handle):
        StorylandMissionEnvironment::VehicleContacts{};
    if(vehicle.spawnedSlot<0||vehicle.model<0||!contacts.valid) {
        error="The car needs an active physics state before fitting a bomb.";return false;
    }
    if(!std::isfinite(vehicle.speed)||!std::isfinite(contacts.speed)||
        std::fabs(vehicle.speed)>.1f||std::fabs(contacts.speed)>.1f) {
        error="Stop the car before fitting a bomb.";return false;
    }
    if(vehicle.bombType) {error="This car already has a bomb fitted.";return false;}
    if(!world.freeBombs&&world.money<500) {error="Fitting a bomb costs $500.";return false;}
    storylandInstallVehicleBomb(world,handle,vehicle,int32_t(mode));
    vehicle.betaBombInstalled=true;
    if(!world.freeBombs)world.money-=500;
    return true;
}

inline bool storylandRemoveBetaVehicleBomb(StorylandMissionEnvironment& world,int32_t handle) {
    const auto found=world.missionVehicles.find(handle);
    if(found==world.missionVehicles.end()||!found->second.betaBombInstalled)return false;
    storylandClearVehicleBomb(found->second);
    return true;
}

inline size_t storylandRemoveBetaVehicleBombs(StorylandMissionEnvironment& world) {
    size_t removed=0;
    for(auto& entry:world.missionVehicles)if(entry.second.betaBombInstalled) {
        storylandClearVehicleBomb(entry.second);++removed;
    }
    return removed;
}

inline bool storylandGarageContainsPoint(const StorylandMissionEnvironment::Garage& garage,
    const std::array<float,3>& point,float margin=0) {
    const auto& b=garage.bounds;
    const float ax=b[3]-b[0],ay=b[4]-b[1],bx=b[5]-b[0],by=b[6]-b[1];
    const float determinant=ax*by-ay*bx;
    if(std::fabs(determinant)<1e-6f)return false;
    const float x=point[0]-b[0],y=point[1]-b[1];
    const float u=(x*by-y*bx)/determinant,v=(ax*y-ay*x)/determinant;
    const float du=margin/std::max(.001f,std::hypot(ax,ay));
    const float dv=margin/std::max(.001f,std::hypot(bx,by));
    return u>=-du&&u<=1+du&&v>=-dv&&v<=1+dv&&
        point[2]>=std::min(b[2],b[7])-margin&&point[2]<=std::max(b[2],b[7])+margin;
}

inline bool storylandGarageContainsVehicle(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Garage& garage,int32_t handle) {
    const auto found=world.missionVehicles.find(handle);
    if(found==world.missionVehicles.end()||found->second.deleted||!world.vehicleWorldCorners)return false;
    std::array<std::array<float,3>,8> points{};
    return world.vehicleWorldCorners(handle,points)&&std::all_of(points.begin(),points.end(),
        [&](const auto& point){return storylandGarageContainsPoint(garage,point);});
}

inline int storylandImportGarageIndex(int type,bool vcs=true) {
    if(!vcs)return type==8?0:type==9?1:type==10?2:type==22?3:-1;
    return type==6?0:type==7?1:type==8?2:type==20?3:-1;
}

inline int32_t storylandLcsImportGarageModel(const StorylandMissionEnvironment& world,int slot) {
    static constexpr std::array<const char*,16> names{
        "hearse","faggio","freeway","spider","manana","shelby","pontiac","esprit",
        "hotrod","pcj600","sentinel","infernus","banshee","patriot","bfinject","landstal"};
    return slot>=0&&size_t(slot)<names.size()&&world.modelIdByName?world.modelIdByName(names[size_t(slot)]):-1;
}

inline bool storylandVcsHideoutGarage(int type) {
    return (type>=14&&type<=16)||(type>=22&&type<=30);
}

// Read-only membership check for the world renderer. Avoid constructing a
// temporary std::set for every visible building in each rendering pass.
inline bool storylandIsNativeGarageDoorModel(const std::vector<uint8_t>& game,
    int32_t model) {
    if (model < 0 || model >= int32_t(UINT16_MAX) || game.size() < 0x60u) return false;
    uint32_t models = 0;
    std::memcpy(&models, game.data() + 0x5Cu, sizeof(models));
    if (!models || uint64_t(models) + 0x12Cu > game.size()) return false;
    for (size_t offset = 0x54u; offset <= 0x80u; offset += 2u) {
        uint16_t candidate = 0;
        std::memcpy(&candidate, game.data() + models + offset, sizeof(candidate));
        if (candidate == uint16_t(model)) return true;
    }
    uint16_t last = 0;
    std::memcpy(&last, game.data() + models + 0x12Au, sizeof(last));
    return last == uint16_t(model);
}

inline std::set<int32_t> storylandNativeGarageDoorModels(const std::vector<uint8_t>& game) {
    std::set<int32_t> result;
    if(game.size()<0x60)return result;
    uint32_t models=0;std::memcpy(&models,game.data()+0x5C,4);
    if(!models||uint64_t(models)+0x12C>game.size())return result;
    const auto insert=[&](size_t offset) {
        uint16_t model=0;std::memcpy(&model,game.data()+models+offset,2);
        if(model!=UINT16_MAX)result.insert(model);
    };
    for(size_t offset=0x54;offset<=0x80;offset+=2)insert(offset);
    insert(0x12A);return result;
}

inline std::array<float,16> storylandGarageDoorMatrix(const StorylandMissionEnvironment::Garage& garage,
    const std::array<float,16>& original,float height) {
    auto matrix=original;
    const float amount=std::clamp(garage.doorPosition,0.0f,1.0f);
    float doorHeight=std::max(0.0f,height-.1f);
    if(garage.swingDoor)doorHeight=std::max(0.0f,doorHeight*.5f-.1f);
    const float travel=doorHeight*amount;
    matrix[14]+=travel;
    if(garage.swingDoor) {
        // CGarage::UpdateDoorsHeight0x423FE8 and rotation helper0x424270.
        matrix[12]+=original[5]*travel*(5.0f/6.0f);
        matrix[13]-=original[4]*travel*(5.0f/6.0f);
        const float angle=-amount*1.57079632679f;
        const std::array<float,3> up{-std::sin(angle)*original[5],std::sin(angle)*original[4],std::cos(angle)};
        matrix[0]=up[1]*original[6]-up[2]*original[5];
        matrix[1]=up[2]*original[4]-up[0]*original[6];
        matrix[2]=up[0]*original[5]-up[1]*original[4];
        matrix[8]=up[0];matrix[9]=up[1];matrix[10]=up[2];
    }
    return matrix;
}

inline bool storylandNativeRopeDefinition(const std::vector<uint8_t>& game,int32_t type,
    float& length,int32_t& hook) {
    // SLUS_215.90 0x486EA0; native rope has31 segments of tableLength/32.
    static constexpr float lengths[]={0,20,4,6,6};
    if(type<1||type>4)return false;
    length=lengths[type]*31.0f/32.0f;hook=-1;
    if(type==1)return true;
    if(game.size()<0x60)return false;
    uint32_t models=0;std::memcpy(&models,game.data()+0x5C,4);
    const size_t offset=type==2?0x90:type==3?0x8C:0x8E;
    if(!models||uint64_t(models)+offset+2>game.size())return false;
    uint16_t model=0;std::memcpy(&model,game.data()+models+offset,2);
    hook=model;return model!=UINT16_MAX;
}

inline bool storylandExplodeScriptVehicle(StorylandMissionEnvironment& world,int32_t handle,
    std::string& error) {
    auto found=world.missionVehicles.find(handle);
    if(found==world.missionVehicles.end()||found->second.deleted||found->second.wrecked)return true;
    auto& vehicle=found->second;
    const int nativeType=world.vehicleNativeType?world.vehicleNativeType(handle):0;
    const int explosionType=nativeType==1||nativeType==2?5:nativeType==4||nativeType==5?6:3;
    if(!storylandAddScriptExplosion(world,explosionType,{vehicle.x,vehicle.y,vehicle.z},error,
        {StorylandMissionEnvironment::DamageSourceKind::Vehicle,handle}))return false;
    vehicle.health=0;vehicle.healthChangedByScript=true;vehicle.alive=false;vehicle.wrecked=true;
    storylandClearVehicleBomb(vehicle);
    vehicle.speed=0;vehicle.velocity={};vehicle.velocityChangedByScript=true;
    if(world.remoteVehicle==handle)world.remoteVehicle=-1;
    return true;
}

inline void storylandRemoveVehicleRope(StorylandMissionEnvironment& world,int32_t owner) {
    const auto found=world.vehicleRopes.find(owner);
    if(found!=world.vehicleRopes.end()) {
        const auto rope=found->second;
        if(rope.hookObject>=0)storylandDeleteScriptObject(world,rope.hookObject);
        if(rope.attachedObject>=0) {
            const auto object=std::find_if(world.objects.begin(),world.objects.end(),
                [&](const auto& item){return item.handle==rope.attachedObject;});
            if(object!=world.objects.end()) {object->dynamic=true;object->velocity=rope.velocity;}
        }
        if(rope.attachedVehicle>=0) {
            const auto car=world.missionVehicles.find(rope.attachedVehicle);
            if(car!=world.missionVehicles.end()) {
                car->second.velocity=rope.velocity;car->second.velocityChangedByScript=true;
            }
        }
        world.vehicleRopes.erase(found);
    }
    const auto car=world.missionVehicles.find(owner);
    if(car!=world.missionVehicles.end())car->second.ropeType=0;
}

inline bool storylandCreateVehicleRope(StorylandMissionEnvironment& world,int32_t owner,
    int32_t type,std::string& error) {
    auto vehicle=world.missionVehicles.find(owner);
    if(vehicle==world.missionVehicles.end()||vehicle->second.deleted)return false;
    if(type==0) {storylandRemoveVehicleRope(world,owner);return true;}
    float length=0;int32_t model=-1;
    std::array<float,16> matrix{};
    if(!world.nativeRopeDefinition||!world.nativeRopeDefinition(type,length,model)||
        !world.vehicleWorldMatrix||!world.vehicleWorldMatrix(owner,matrix)) {
        error="Vehicle rope needs a native type1..4 and a live vehicle matrix.";return false;
    }
    const auto existing=world.vehicleRopes.find(owner);
    if(existing==world.vehicleRopes.end()&&world.vehicleRopes.size()>=8)return true; // Native pool full.
    StorylandMissionEnvironment::VehicleRope rope;
    rope.owner=owner;rope.type=type;rope.length=length;
    const auto& offset=vehicle->second.ropeOffset;
    for(size_t axis=0;axis<3;++axis)
        rope.anchor[axis]=matrix[12+axis]+matrix[axis]*offset[0]+matrix[4+axis]*offset[1]+matrix[8+axis]*offset[2];
    rope.end=rope.anchor;rope.end[2]-=rope.length;
    StorylandMissionEnvironment::Object hook;
    if(model>=0) {
        if(world.objects.size()>=4096||world.nextObjectHandle<=0||world.nextObjectHandle==INT32_MAX) {
            error="Vehicle rope needs a free object pool slot for its native hook.";return false;
        }
        hook.handle=world.nextObjectHandle;hook.model=model;
        hook.x=rope.end[0];hook.y=rope.end[1];hook.z=rope.end[2];
        if(!storylandPrepareScriptObject(world,hook,false,error))return false;
        hook.creationSource=5;hook.collisionEnabled=false;hook.gravity=false;hook.persistent=true;
        rope.hookObject=hook.handle;
    }
    storylandRemoveVehicleRope(world,owner);
    if(model>=0) {world.objects.push_back(hook);++world.nextObjectHandle;}
    vehicle->second.ropeType=type;world.vehicleRopes.emplace(owner,rope);
    return true;
}

inline void storylandAdvanceVehicleRopes(StorylandMissionEnvironment& world,float seconds,bool vcs) {
    if(!std::isfinite(seconds)||seconds<=0)return;
    const float dt=std::min(seconds,.1f);
    std::vector<int32_t> removed;
    for(auto& entry:world.vehicleRopes) {
        auto& rope=entry.second;
        const auto car=world.missionVehicles.find(rope.owner);
        std::array<float,16> matrix{};
        if(car==world.missionVehicles.end()||car->second.deleted||car->second.ropeType==0) {
            removed.push_back(entry.first);continue;
        }
        if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(rope.owner,matrix))continue;
        const auto& offset=car->second.ropeOffset;
        for(size_t axis=0;axis<3;++axis)
            rope.anchor[axis]=matrix[12+axis]+matrix[axis]*offset[0]+matrix[4+axis]*offset[1]+matrix[8+axis]*offset[2];
        const auto previous=rope.end;
        rope.velocity[2]-=20*dt;
        for(size_t axis=0;axis<3;++axis)rope.end[axis]+=rope.velocity[axis]*dt;
        std::array<float,3> delta{};
        for(size_t axis=0;axis<3;++axis)delta[axis]=rope.end[axis]-rope.anchor[axis];
        const float distance=std::hypot(delta[0],delta[1],delta[2]);
        if(distance>rope.length&&distance>0)
            for(size_t axis=0;axis<3;++axis)rope.end[axis]=rope.anchor[axis]+delta[axis]*rope.length/distance;
        float ground=0;
        if(world.groundHeightAt&&world.groundHeightAt(rope.end,ground)&&rope.end[2]<ground)rope.end[2]=ground;
        for(size_t axis=0;axis<3;++axis)rope.velocity[axis]=(rope.end[axis]-previous[axis])/dt*.97f;
        bool vcsReleasedPayload=false;
        auto object=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& item){return item.handle==rope.attachedObject;});
        if(rope.attachedObject>=0&&(object==world.objects.end()||!object->ropeLiftable)) {
            if(object!=world.objects.end()) {object->dynamic=true;object->velocity=rope.velocity;}
            rope.attachedObject=-1;
            vcsReleasedPayload=vcs;
        }
        auto payload=world.missionVehicles.find(rope.attachedVehicle);
        if(rope.attachedVehicle>=0&&(payload==world.missionVehicles.end()||payload->second.deleted||
            !payload->second.ropeLiftable||(world.vehicleNativeType&&world.vehicleNativeType(payload->first)==6&&
            (payload->second.driver>=0||(world.inVehicle&&world.occupiedMissionVehicle==payload->first))))) {
            if(payload!=world.missionVehicles.end()) {
                payload->second.velocity=rope.velocity;payload->second.velocityChangedByScript=true;
            }
            rope.attachedVehicle=-1;
            vcsReleasedPayload=vcs;
        }
        // The VCS release routine stores 60 at rope+0x42c. On each later
        // unladen update the original decrements it and skips both scans.
        bool vcsMayAcquire=true;
        if(vcsReleasedPayload) {
            rope.vcsReacquireDelay=60;
            vcsMayAcquire=false;
        } else if(vcs&&rope.attachedObject<0&&rope.attachedVehicle<0&&rope.vcsReacquireDelay>0) {
            --rope.vcsReacquireDelay;
            vcsMayAcquire=false;
        }
        // Native types2..4 have a hook which acquires eligible touching objects/cars.
        if(vcsMayAcquire&&rope.hookObject>=0&&rope.attachedObject<0&&rope.attachedVehicle<0) {
            float nearest=1.2f;
            if(!vcs)for(auto& item:world.objects) {
                if(!item.ropeLiftable||item.handle==rope.hookObject||item.attachedVehicle>=0||item.attachedCharacter>=0)continue;
                if(std::any_of(world.vehicleRopes.begin(),world.vehicleRopes.end(),[&](const auto& other) {
                    return other.first!=entry.first&&other.second.attachedObject==item.handle;
                }))continue;
                const float d=std::hypot(item.x-rope.end[0],item.y-rope.end[1],item.z-rope.end[2]);
                if(d<nearest) {nearest=d;rope.attachedObject=item.handle;rope.attachedVehicle=-1;}
            }
            for(auto& item:world.missionVehicles) {
                auto& candidate=item.second;
                if(item.first==rope.owner||candidate.deleted||!candidate.ropeLiftable)continue;
                if(std::any_of(world.vehicleRopes.begin(),world.vehicleRopes.end(),[&](const auto& other) {
                    return other.first!=entry.first&&other.second.attachedVehicle==item.first;
                }))continue;
                const int type=world.vehicleNativeType?world.vehicleNativeType(item.first):-1;
                if(type!=0&&type!=6)continue;
                if(type==6&&(candidate.driver>=0||(world.inVehicle&&world.occupiedMissionVehicle==item.first)))continue;
                std::array<std::array<float,3>,8> points{};
                if(!world.vehicleWorldCorners||!world.vehicleWorldCorners(item.first,points))continue;
                std::array<float,3> roof{};for(size_t i=4;i<8;++i)for(size_t a=0;a<3;++a)roof[a]+=points[i][a]*.25f;
                const float d=std::hypot(roof[0]-rope.end[0],roof[1]-rope.end[1],roof[2]-rope.end[2]);
                if(d<nearest) {nearest=d;rope.attachedVehicle=item.first;rope.attachedObject=-1;}
            }
            // Native 0x226FD8 checks vehicles before the 0x227E80 object scan.
            if(vcs&&rope.attachedVehicle<0)storylandAcquireVcsRopeObject(world,rope);
        }
        if(vcs&&rope.attachedObject>=0) {
            storylandMoveVcsRopeObject(world,rope);
        } else for(auto& item:world.objects) {
            if(item.handle!=rope.hookObject&&item.handle!=rope.attachedObject)continue;
            item.x=rope.end[0];item.y=rope.end[1];item.z=rope.end[2];
            item.velocity=rope.velocity;item.dynamic=false;
        }
        payload=world.missionVehicles.find(rope.attachedVehicle);
        if(payload!=world.missionVehicles.end()) {
            auto& item=payload->second;
            float height=1.0f;
            std::array<std::array<float,3>,8> points{};
            if(world.vehicleWorldCorners&&world.vehicleWorldCorners(payload->first,points))
                height=std::max(.1f,(points[4][2]+points[5][2]+points[6][2]+points[7][2])*.25f-item.z);
            item.x=rope.end[0];item.y=rope.end[1];item.z=rope.end[2]-height;
            item.movedByScript=true;item.velocity=rope.velocity;item.velocityChangedByScript=true;
        }
    }
    for(int32_t owner:removed)storylandRemoveVehicleRope(world,owner);
}

inline bool storylandAdvanceGarages(StorylandMissionEnvironment& world,float seconds,bool vcs=true,
    std::string* error=nullptr) {
    if(error)error->clear();
    if(!std::isfinite(seconds)||seconds<=0)return true;
    const float dt=std::min(seconds,.1f);
    for(auto& garage:world.garages) {
        if(vcs?storylandVcsHideoutGarage(garage.type):
            ((garage.type>=16&&garage.type<=18)||(garage.type>=24&&garage.type<=32)||garage.type==13))continue;
        const int import=storylandImportGarageIndex(garage.type,vcs);
        const bool service=garage.type>=2&&garage.type<=5;
        if(!vcs&&!service&&garage.targetVehicle<0&&import<0)continue;
        const auto failedBinding=[&](const std::string& reason) {
            if(error)*error="Native VCS mission garage "+std::to_string(garage.handle)+": "+reason;
            return false;
        };
        if(vcs&&garage.type==1&&garage.state==0) {
            if(!garage.locked&&world.inVehicle&&garage.targetVehicle>=0&&
                world.occupiedMissionVehicle==garage.targetVehicle) {
                const auto target=world.missionVehicles.find(garage.targetVehicle);
                if(target!=world.missionVehicles.end()&&!target->second.deleted) {
                    std::array<float,16> matrix{};
                    if(!world.vehicleWorldMatrix||!world.vehicleWorldMatrix(garage.targetVehicle,matrix)||
                        !std::isfinite(matrix[12])||!std::isfinite(matrix[13]))
                        return failedBinding("the occupied target has no valid native entity matrix.");
                    if(storylandVcsMissionGarageCanOpen(garage.locked,world.inVehicle,
                        garage.targetVehicle,world.occupiedMissionVehicle,garage.bounds,
                        {matrix[12],matrix[13],matrix[14]}))garage.state=3;
                }
            }
            continue;
        }
        StorylandMissionEnvironment::LcsGarageDoorState missionDoor;
        const bool nativeMissionMotion=vcs&&garage.type==1&&(garage.state==2||garage.state==3);
        if(nativeMissionMotion) {
            std::string bindingError;
            if(!world.prepareLcsGarageDoors||!world.prepareLcsGarageDoors(garage,missionDoor,bindingError))
                return failedBinding(bindingError.empty()?"the original door placement binding is unavailable.":bindingError);
            if(!std::isfinite(missionDoor.height)||missionDoor.height<=0||
                !std::isfinite(missionDoor.position))
                return failedBinding("the original door travel is invalid.");
        }
        if(!vcs&&!world.lcsGarageDoors.count(garage.handle)&&world.prepareLcsGarageDoors) {
            StorylandMissionEnvironment::LcsGarageDoorState prepared;
            std::string error;
            if(world.prepareLcsGarageDoors(garage,prepared,error))
                world.lcsGarageDoors.emplace(garage.handle,std::move(prepared));
        }
        const auto nativeDoor=world.lcsGarageDoors.find(garage.handle);
        const bool nativeMotion=!vcs&&nativeDoor!=world.lcsGarageDoors.end();
        if(nativeMotion) {
            if(garage.state==2&&nativeDoor->second.state==0&&garage.serviceVehicle>=0&&garage.serviceUntil)
                garage.serviceUntil=world.gameMilliseconds+2000;
            if(garage.state!=5)garage.state=uint8_t(nativeDoor->second.state);
            garage.doorPosition=nativeDoor->second.height>0?nativeDoor->second.position/nativeDoor->second.height:0;
        }
        const auto syncDoor=[&]() {
            if(nativeMotion)nativeDoor->second.state=garage.state;
        };
        if(garage.locked&&garage.state==1)garage.state=2;
        if(service&&!garage.locked&&garage.serviceVehicle<0&&garage.state==0&&
            storylandGarageContainsPoint(garage,{world.x,world.y,world.z},10))garage.state=3;
        if(!nativeMotion&&garage.state==3) {
            garage.doorPosition=nativeMissionMotion?
                storylandVcsMissionGarageDoorPosition(missionDoor.position,missionDoor.height,
                    dt*50.0f,garage.swingDoor,true)/missionDoor.height:
                std::min(1.0f,garage.doorPosition+dt*.5f);
            if(garage.doorPosition>=1)garage.state=1;
            if(nativeMissionMotion)continue;
        } else if(!nativeMotion&&garage.state==2) {
            garage.doorPosition=nativeMissionMotion?
                storylandVcsMissionGarageDoorPosition(missionDoor.position,missionDoor.height,
                    dt*50.0f,garage.swingDoor,false)/missionDoor.height:
                std::max(0.0f,garage.doorPosition-dt*.5f);
            if(garage.doorPosition<=0) {
                garage.state=0;
                if(garage.serviceVehicle>=0&&garage.serviceUntil)
                    garage.serviceUntil=world.gameMilliseconds+2000;
            }
        }
        if(garage.targetVehicle>=0&&garage.state!=5&&!world.inVehicle&&
            storylandGarageContainsVehicle(world,garage,garage.targetVehicle)&&
            !storylandGarageContainsPoint(garage,{world.x,world.y,world.z},.5f)) {
            garage.state=2;
            if(garage.doorPosition<=0) {
                garage.state=5;
                auto& car=world.missionVehicles.at(garage.targetVehicle);
                if(vcs||(garage.type!=14&&garage.type!=21)) {
                    car.deleted=true;car.alive=false;car.visible=false;car.collisionEnabled=false;
                    world.vcsVehicleFlag28.erase(garage.targetVehicle);
                    world.vcsGarageStorableMissionVehicles.erase(garage.targetVehicle);
                    world.releaseCharacterVehicleReferences(garage.targetVehicle);
                }
            }
        }
        if(import==0&&garage.state==1&&(world.garageImportModel||(!vcs&&world.modelIdByName))&&!world.inVehicle) {
            for(auto& entry:world.missionVehicles) {
                auto& car=entry.second;
                if(car.deleted||!storylandGarageContainsVehicle(world,garage,entry.first))continue;
                for(int slot=0;slot<16;++slot) {
                    const int32_t model=world.garageImportModel?world.garageImportModel(garage.type,slot):
                        storylandLcsImportGarageModel(world,slot);
                    if(model!=car.model)continue;
                    const uint32_t mask=uint32_t(1)<<slot;
                    if(!(world.importGarageMasks[0]&mask)) {
                        world.importGarageMasks[0]|=mask;garage.deliveredModelMask|=mask;
                        car.deleted=true;car.alive=false;car.visible=false;car.collisionEnabled=false;
                        world.vcsVehicleFlag28.erase(entry.first);
                        world.vcsGarageStorableMissionVehicles.erase(entry.first);
                        world.releaseCharacterVehicleReferences(entry.first);
                    }
                }
            }
        }
        if(!service) {syncDoor();continue;}
        auto current=world.missionVehicles.find(garage.serviceVehicle);
        if(garage.serviceVehicle>=0) {
            if(current==world.missionVehicles.end()||current->second.deleted||
                !storylandGarageContainsVehicle(world,garage,garage.serviceVehicle)) {
                garage.serviceVehicle=-1;garage.serviceUntil=0;syncDoor();continue;
            }
            if(garage.state==0&&garage.serviceUntil&&int32_t(world.gameMilliseconds-garage.serviceUntil)>=0) {
                auto& car=current->second;
                if(garage.type==5) {
                    if(!car.canRespray) {garage.serviceUntil=0;garage.state=3;syncDoor();continue;}
                    if(!world.freeResprays)world.money=std::max(0,world.money-100);
                    car.health=car.maximumHealth;car.healthChangedByScript=true;car.wrecked=false;car.alive=true;
                    car.tyreStatus={};car.doorDamage={};car.panelDamage={};car.lightDamage={};
                    car.doorOpenRatios={};car.doorsChangedByScript=true;car.visiblyDamaged=false;
                    car.extinguishRequested=true;
                    const int sets=world.vehicleColorSetCount?world.vehicleColorSetCount(garage.serviceVehicle,false):0;
                    if(sets>0&&world.vehicleColorSet) {
                        const int next=(std::max(0,car.colorSet)+1)%sets;
                        std::array<int32_t,2> colors{};
                        if(world.vehicleColorSet(garage.serviceVehicle,false,next,colors)) {
                            car.colorSet=next;car.scriptColorSet=-1;car.primaryColor=colors[0];car.secondaryColor=colors[1];
                        }
                    }
                    if(!vcs&&world.lcsExtendedWorld.suspendWanted)world.lcsExtendedWorld.suspendWanted();
                    world.wanted=0;garage.resprayDone=true;
                } else {
                    if(!world.freeBombs)world.money=std::max(0,world.money-500);
                    storylandInstallVehicleBomb(world,current->first,car,garage.type-1);
                }
                garage.serviceUntil=0;garage.state=3;
            }
        } else if(garage.state==1&&world.inVehicle&&world.playerPassengerSeat<0&&
            storylandGarageContainsVehicle(world,garage,world.occupiedMissionVehicle)) {
            auto& car=world.missionVehicles.at(world.occupiedMissionVehicle);
            if(std::fabs(car.speed)>.1f||car.health<=0)continue;
            if(garage.type==5&&(!car.canRespray||(!world.freeResprays&&world.money<100)))continue;
            if(garage.type!=5&&(car.bombType!=0||(!world.freeBombs&&world.money<500)||
                !world.vehicleNativeType||world.vehicleNativeType(world.occupiedMissionVehicle)!=0))continue;
            garage.serviceVehicle=world.occupiedMissionVehicle;
            garage.serviceUntil=world.gameMilliseconds+4000; // Rebased to two seconds when the door closes.
            garage.state=2;
        }
        syncDoor();
    }
    if(vcs&&world.updateGarageDoor)for(const auto& garage:world.garages)
        world.updateGarageDoor(garage);
    return true;
}
