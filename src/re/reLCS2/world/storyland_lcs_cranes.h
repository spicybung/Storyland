#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

using StorylandLcsCraneMatrix = std::array<float,16>;

struct StorylandLcsCraneVehicle {
    int32_t handle=-1,model=-1;
    StorylandLcsCraneMatrix matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,3> velocity{};
    float roofHeight=0,health=1000;
    bool automobile=true,wrecked=false,hasDriver=false,beingEntered=false;
};

struct StorylandLcsCrane {
    enum Status : uint8_t { None, Activated, Deactivated };
    enum State : uint8_t { Idle, Approaching, Lifting, Raising, Aligning, Dropping };
    uint64_t worldKey=0;
    int32_t model=-1;
    uint16_t randomSeed=0;
    StorylandLcsCraneMatrix originalMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,4> pickup{}; // Minimum X/Y, maximum X/Y.
    std::array<float,3> dropoff{};
    float dropoffHeading=0;
    float pickupAngle=0,dropoffAngle=0,pickupDistance=0,dropoffDistance=0;
    float pickupHeight=0,dropoffHeight=0;
    float hookAngle=0,hookOffset=20,hookHeight=20;
    std::array<float,3> hookPosition{};
    std::array<float,2> hookVelocity{};
    int32_t vehicle=-1;
    uint32_t nextCheck=0;
    uint8_t status=None,state=Idle,collected=0;
    bool crusher=false,military=false,wasMilitary=false,top=false;

    template<class Archive> void saveState(Archive& archive) {
        archive(worldKey,model,randomSeed,originalMatrix,pickup,dropoff,dropoffHeading,
            pickupAngle,dropoffAngle,pickupDistance,dropoffDistance,pickupHeight,dropoffHeight,
            hookAngle,hookOffset,hookHeight,hookPosition,hookVelocity,vehicle,nextCheck,
            status,state,collected,crusher,military,wasMilitary,top);
    }
};

struct StorylandLcsCraneBindings {
    std::function<bool(std::vector<StorylandLcsCrane>&,std::string&)> discover;
    std::function<bool(const StorylandLcsCrane&,const StorylandLcsCraneMatrix&,std::string&)> transform;
    std::function<bool(const std::array<float,4>&,std::vector<StorylandLcsCraneVehicle>&,std::string&)> findVehicles;
    std::function<bool(int32_t,StorylandLcsCraneVehicle&,std::string&)> readVehicle;
    std::function<bool(int32_t,const StorylandLcsCraneMatrix&,bool,bool,std::string&)> updateVehicle;
    std::function<bool(int32_t,std::string&)> removeVehicle;
    std::function<bool(const std::array<float,3>&,float&)> groundHeight;
    std::function<void(const std::string&,int32_t)> message;
    std::function<void(int32_t)> reward;
};

struct StorylandLcsCraneSystem {
    std::vector<StorylandLcsCrane> cranes;
    uint32_t militaryCars=0;
    std::set<int32_t> notifiedVehicles;
    bool initialized=false;
    StorylandLcsCraneBindings bindings;

    template<class Archive> void saveState(Archive& archive) {
        archive(cranes,militaryCars,notifiedVehicles,initialized);
    }
};

inline constexpr float StorylandLcsCranePi=3.14159265358979323846f;

inline bool storylandLcsCraneFiniteMatrix(const StorylandLcsCraneMatrix& matrix) {
    return std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);})&&
        matrix[3]==0&&matrix[7]==0&&matrix[11]==0&&matrix[15]==1;
}

inline bool storylandValidLcsCraneSystem(const StorylandLcsCraneSystem& system) {
    if(system.cranes.size()>11||system.militaryCars>0x7F||system.notifiedVehicles.size()>4096||
        (!system.initialized&&(!system.cranes.empty()||system.militaryCars||!system.notifiedVehicles.empty())))return false;
    const auto finite=[](const auto& values) {
        return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
    };
    std::set<uint64_t> keys;
    for(const auto& crane:system.cranes) {
        if(crane.worldKey==0||!keys.insert(crane.worldKey).second||crane.model<0||
            !storylandLcsCraneFiniteMatrix(crane.originalMatrix)||crane.status>StorylandLcsCrane::Deactivated||
            crane.state>StorylandLcsCrane::Dropping||crane.vehicle<-1||(crane.crusher&&crane.military)||
            (crane.military&&!crane.wasMilitary)||crane.pickup[0]>crane.pickup[2]||crane.pickup[1]>crane.pickup[3])return false;
        if(!finite(crane.pickup)||!finite(crane.dropoff)||!finite(crane.hookPosition)||!finite(crane.hookVelocity))return false;
        for(float value:{crane.dropoffHeading,crane.pickupAngle,crane.dropoffAngle,crane.pickupDistance,
            crane.dropoffDistance,crane.pickupHeight,crane.dropoffHeight,crane.hookAngle,
            crane.hookOffset,crane.hookHeight})if(!std::isfinite(value))return false;
        if(crane.pickupDistance<0||crane.dropoffDistance<0||crane.hookOffset<0)return false;
        for(float angle:{crane.pickupAngle,crane.dropoffAngle,crane.hookAngle})
            if(angle<0||angle>2*StorylandLcsCranePi)return false;
        if((crane.status==StorylandLcsCrane::None&&
            (crane.state!=StorylandLcsCrane::Idle||crane.vehicle>=0))||
            (crane.state==StorylandLcsCrane::Dropping&&crane.vehicle>=0))return false;
        if(crane.state>=StorylandLcsCrane::Approaching&&crane.state<=StorylandLcsCrane::Aligning&&
            crane.vehicle<0)return false;
    }
    for(int32_t handle:system.notifiedVehicles)if(handle<0)return false;
    return true;
}

template<class NativeWorld> bool storylandDiscoverLcsCranes(const std::vector<uint8_t>& data,
    const NativeWorld& native,std::vector<StorylandLcsCrane>& result,std::string& error) {
    const auto valid=[&](uint64_t offset,uint64_t bytes){return offset<=data.size()&&bytes<=data.size()-offset;};
    const auto word=[&](size_t offset) {
        return uint32_t(data[offset])|uint32_t(data[offset+1])<<8|
            uint32_t(data[offset+2])<<16|uint32_t(data[offset+3])<<24;
    };
    if(!valid(0,0x60)||native.instances.empty()) {
        error="Crane discovery requires the native LCS model table and building placements.";return false;
    }
    const uint32_t table=word(0x5C),modelCount=word(0x38);
    if(table==0||!valid(table,36*2)) {error="The native LCS crane model table is truncated.";return false;}
    std::array<int32_t,6> models{};
    for(size_t index=0;index<models.size();++index) {
        const size_t at=size_t(table)+(30+index)*2;
        models[index]=int16_t(uint16_t(data[at])|uint16_t(data[at+1])<<8);
    }
    std::vector<StorylandLcsCrane> discovered;
    for(const auto& instance:native.instances) {
        if((instance.poolHeader!=0x24&&instance.poolHeader!=0x28)||instance.modelId>=modelCount||
            std::find(models.begin(),models.end(),int32_t(instance.modelId))==models.end())continue;
        if(discovered.size()==11)break;
        const uint32_t pool=word(instance.poolHeader);
        if(pool==0||!valid(pool,16)) {error="Native crane building pool is truncated.";return false;}
        const uint32_t items=word(pool),count=word(pool+8);
        const uint64_t row=uint64_t(items)+uint64_t(instance.poolIndex)*96;
        if(items==0||instance.poolIndex>=count||!valid(row,96)) {
            error="Native crane building identity is outside its pool.";return false;
        }
        StorylandLcsCrane crane;
        crane.worldKey=uint64_t(instance.poolHeader)<<32|instance.poolIndex;
        crane.model=int32_t(instance.modelId);
        crane.randomSeed=uint16_t(data[size_t(row)+0x56])|uint16_t(data[size_t(row)+0x57])<<8;
        const auto original=native.originalScriptInstanceMatrices.find(crane.worldKey);
        crane.originalMatrix=original==native.originalScriptInstanceMatrices.end()?instance.matrix:original->second;
        crane.top=crane.model!=models[0];
        discovered.push_back(crane);
    }
    result=std::move(discovered);
    error.clear();return true;
}

inline float storylandLcsCraneAngle(float x,float y) {
    const float angle=std::atan2(y,x);
    return angle<0?angle+2*StorylandLcsCranePi:angle;
}

inline std::array<float,3> storylandLcsCraneHook(const StorylandLcsCrane& crane) {
    return {std::cos(crane.hookAngle)*crane.hookOffset+crane.originalMatrix[12],
        std::sin(crane.hookAngle)*crane.hookOffset+crane.originalMatrix[13],crane.hookHeight};
}

inline StorylandLcsCraneMatrix storylandLcsCraneFrame(const StorylandLcsCrane& crane) {
    const float cosine=std::cos(crane.hookAngle),sine=std::sin(crane.hookAngle);
    return {cosine,sine,0,0,-sine,cosine,0,0,0,0,1,0,
        crane.originalMatrix[12],crane.originalMatrix[13],crane.originalMatrix[14],1};
}

inline bool storylandLcsCraneBindingsReady(const StorylandLcsCraneSystem& system,
    bool military,std::string& error) {
    const auto& binding=system.bindings;
    if((!system.initialized&&!binding.discover)||!binding.transform||!binding.findVehicles||
        !binding.readVehicle||!binding.updateVehicle||!binding.message||
        (military&&(!binding.removeVehicle||!binding.reward))) {
        error="Crane operations require native buildings, vehicle physics, rendering and message bindings.";
        return false;
    }
    return true;
}

inline bool storylandInitializeLcsCranes(StorylandLcsCraneSystem& system,std::string& error) {
    if(system.initialized)return true;
    if(!system.bindings.discover) {error="Native crane discovery is unavailable.";return false;}
    std::vector<StorylandLcsCrane> cranes;
    if(!system.bindings.discover(cranes,error))return false;
    for(size_t index=0;index<cranes.size();++index) {
        auto& crane=cranes[index];
        crane.hookAngle=std::fmod(float(index),2*StorylandLcsCranePi);
        crane.hookPosition={}; // CCrane::Init clears this; the first update positions the hook.
    }
    StorylandLcsCraneSystem candidate;
    candidate.cranes=std::move(cranes);candidate.initialized=true;
    if(!storylandValidLcsCraneSystem(candidate)) {error="Native crane identities or transforms are invalid.";return false;}
    system.cranes=std::move(candidate.cranes);system.initialized=true;
    return true;
}

inline size_t storylandFindLcsCrane(const StorylandLcsCraneSystem& system,float x,float y) {
    float nearest=10000.0f;
    size_t selected=system.cranes.size();
    for(size_t index=0;index<system.cranes.size();++index) {
        const auto& crane=system.cranes[index];
        const float dx=x-crane.originalMatrix[12],dy=y-crane.originalMatrix[13];
        const float distance=dx*dx+dy*dy;
        if(distance<nearest) {nearest=distance;selected=index;}
    }
    return selected;
}

inline bool storylandActivateLcsCrane(StorylandLcsCraneSystem& system,
    const std::array<float,10>& parameters,bool crusher,bool military,std::string& error) {
    if(!std::all_of(parameters.begin(),parameters.end(),[](float value){return std::isfinite(value);})||
        (crusher&&military)) {error="Crane activation contains invalid coordinates or mode.";return false;}
    if(!storylandLcsCraneBindingsReady(system,military,error))return false;
    StorylandLcsCraneSystem candidate=system;
    if(!storylandInitializeLcsCranes(candidate,error))return false;
    const bool useDropoff=parameters[0]<=-10000.0f||parameters[1]<=-10000.0f;
    const size_t index=storylandFindLcsCrane(candidate,parameters[useDropoff?6:0],parameters[useDropoff?7:1]);
    if(index==candidate.cranes.size()) {error="No native crane is within 100 units of the requested location.";return false;}
    auto& crane=candidate.cranes[index];
    const int32_t previousVehicle=crane.vehicle;
    const bool wasCarrying=crane.state==StorylandLcsCrane::Lifting||crane.state==StorylandLcsCrane::Raising||
        crane.state==StorylandLcsCrane::Aligning;
    crane.pickup={std::min(parameters[2],parameters[4]),std::min(parameters[3],parameters[5]),
        std::max(parameters[2],parameters[4]),std::max(parameters[3],parameters[5])};
    crane.dropoff={parameters[6],parameters[7],parameters[8]};
    crane.dropoffHeading=parameters[9]*(StorylandLcsCranePi/180.0f);
    crane.crusher=crusher;crane.military=military;crane.wasMilitary=crane.wasMilitary||military;
    const float centerX=(crane.pickup[0]+crane.pickup[2])*.5f;
    const float centerY=(crane.pickup[1]+crane.pickup[3])*.5f;
    float ground=crusher?-0.951f:10.7862f;
    if(!crusher&&!military&&(!candidate.bindings.groundHeight||
        !candidate.bindings.groundHeight({centerX,centerY,0},ground))) {
        error="Crane pickup ground height is unavailable.";return false;
    }
    if(!std::isfinite(ground)) {error="Crane pickup ground height is invalid.";return false;}
    const float pickupX=centerX-crane.originalMatrix[12],pickupY=centerY-crane.originalMatrix[13];
    const float dropoffX=crane.dropoff[0]-crane.originalMatrix[12],dropoffY=crane.dropoff[1]-crane.originalMatrix[13];
    crane.pickupAngle=storylandLcsCraneAngle(pickupX,pickupY);
    crane.dropoffAngle=storylandLcsCraneAngle(dropoffX,dropoffY);
    crane.pickupDistance=std::hypot(pickupX,pickupY);crane.dropoffDistance=std::hypot(dropoffX,dropoffY);
    crane.pickupHeight=ground;crane.dropoffHeight=crane.dropoff[2];
    crane.status=StorylandLcsCrane::Activated;crane.state=StorylandLcsCrane::Idle;
    crane.vehicle=-1;crane.collected=0;crane.nextCheck=0;
    if(!storylandValidLcsCraneSystem(candidate)) {error="Crane activation exceeds the native coordinate range.";return false;}
    if(!candidate.bindings.transform(crane,storylandLcsCraneFrame(crane),error))return false;
    if(wasCarrying&&previousVehicle>=0) {
        StorylandLcsCraneVehicle vehicle;
        std::string vehicleError;
        if(candidate.bindings.readVehicle(previousVehicle,vehicle,vehicleError)) {
            if(!candidate.bindings.updateVehicle(previousVehicle,vehicle.matrix,false,
                system.cranes[index].crusher,vehicleError)&&vehicleError.empty())
                vehicleError="Crane could not release its previous vehicle.";
        }
        if(!vehicleError.empty()) {
            std::string restoreError;
            if(!candidate.bindings.transform(system.cranes[index],
                storylandLcsCraneFrame(system.cranes[index]),restoreError))
                vehicleError+=" Crane transform rollback failed: "+restoreError;
            error=vehicleError;return false;
        }
    }
    system=std::move(candidate);
    error.clear();return true;
}

inline bool storylandDeactivateLcsCrane(StorylandLcsCraneSystem& system,
    float x,float y,std::string& error) {
    if(!std::isfinite(x)||!std::isfinite(y)) {error="Crane deactivation requires finite coordinates.";return false;}
    if(!storylandLcsCraneBindingsReady(system,false,error))return false;
    StorylandLcsCraneSystem candidate=system;
    if(!storylandInitializeLcsCranes(candidate,error))return false;
    const size_t index=storylandFindLcsCrane(candidate,x,y);
    if(index==candidate.cranes.size()) {error="No native crane is within 100 units of the requested location.";return false;}
    auto& crane=candidate.cranes[index];
    if(crane.vehicle>=0&&(crane.state==StorylandLcsCrane::Lifting||crane.state==StorylandLcsCrane::Raising||
        crane.state==StorylandLcsCrane::Aligning)) {
        StorylandLcsCraneVehicle vehicle;
        std::string vehicleError;
        if(candidate.bindings.readVehicle(crane.vehicle,vehicle,vehicleError)) {
            if(!candidate.bindings.updateVehicle(crane.vehicle,vehicle.matrix,false,crane.crusher,error))return false;
        } else if(!vehicleError.empty()) {error=vehicleError;return false;}
    }
    crane.status=StorylandLcsCrane::Deactivated;crane.state=StorylandLcsCrane::Idle;
    system=std::move(candidate);
    error.clear();return true;
}

inline bool storylandLcsCraneLiftingCar(const StorylandLcsCraneSystem& system,
    float x,float y,int32_t vehicle) {
    for(const auto& crane:system.cranes) {
        const float dx=x-crane.originalMatrix[12],dy=y-crane.originalMatrix[13];
        if(dx*dx+dy*dy<10000.0f&&crane.vehicle==vehicle&&
            (crane.state==StorylandLcsCrane::Lifting||crane.state==StorylandLcsCrane::Aligning))return true;
    }
    return false;
}

inline bool storylandLcsCraneMove(float& value,float target,float distance) {
    const float difference=target-value;
    if(std::fabs(difference)<distance) {value=target;return true;}
    value+=difference<0?-distance:distance;
    return false;
}

inline bool storylandLcsCraneMoveTo(StorylandLcsCrane& crane,float angle,float distance,
    float height,float step,float multiplier=1.0f) {
    float difference=angle-crane.hookAngle;
    while(difference>StorylandLcsCranePi)difference-=2*StorylandLcsCranePi;
    while(difference<-StorylandLcsCranePi)difference+=2*StorylandLcsCranePi;
    const float angleStep=multiplier*step*.004f;
    const bool angleDone=std::fabs(difference)<angleStep;
    if(angleDone)crane.hookAngle=angle;
    else {
        crane.hookAngle+=difference<0?-angleStep:angleStep;
        if(crane.hookAngle<0)crane.hookAngle+=2*StorylandLcsCranePi;
        else if(crane.hookAngle>2*StorylandLcsCranePi)crane.hookAngle-=2*StorylandLcsCranePi;
    }
    const bool distanceDone=storylandLcsCraneMove(crane.hookOffset,distance,multiplier*step*.1f);
    const bool heightDone=storylandLcsCraneMove(crane.hookHeight,height,multiplier*step*.06f);
    return angleDone&&distanceDone&&heightDone;
}

inline uint32_t storylandLcsMilitaryCraneBit(int32_t model) {
    // LCS's MI_FIRST_VEHICLE is 130. FBIRANCH remains the native unused -982 entry.
    switch(model) {
    case 138:return 1; // Fire truck.
    case 147:return 2; // Ambulance.
    case 158:return 4; // Enforcer.
    case -982:return 8;
    case 162:return 16; // Rhino.
    case 163:return 32; // Barracks.
    case 157:return 64; // Police.
    default:return 0;
    }
}

inline bool storylandLcsCraneAllowsModel(const StorylandLcsCrane& crane,int32_t model) {
    if(crane.crusher)return model!=138&&model!=139&&model!=165&&model!=159&&
        model!=161&&model!=164&&model!=162;
    return !crane.military||storylandLcsMilitaryCraneBit(model)!=0;
}

inline bool storylandLcsCraneValidVehicle(const StorylandLcsCraneVehicle& vehicle) {
    return vehicle.handle>=0&&std::isfinite(vehicle.roofHeight)&&std::isfinite(vehicle.health)&&
        storylandLcsCraneFiniteMatrix(vehicle.matrix)&&
        std::all_of(vehicle.velocity.begin(),vehicle.velocity.end(),[](float value){return std::isfinite(value);});
}

inline bool storylandLcsCraneInside(const StorylandLcsCrane& crane,const StorylandLcsCraneVehicle& vehicle) {
    return vehicle.matrix[12]>=crane.pickup[0]&&vehicle.matrix[12]<=crane.pickup[2]&&
        vehicle.matrix[13]>=crane.pickup[1]&&vehicle.matrix[13]<=crane.pickup[3];
}

inline bool storylandLcsCraneRotateVehicle(const StorylandLcsCrane& crane,
    StorylandLcsCraneVehicle& vehicle,float step) {
    if(crane.dropoffHeading<=0)return true;
    float difference=crane.dropoffHeading-storylandLcsCraneAngle(vehicle.matrix[4],vehicle.matrix[5]);
    difference=std::fmod(difference,StorylandLcsCranePi);
    if(difference<-StorylandLcsCranePi*.5f)difference+=StorylandLcsCranePi;
    if(difference>StorylandLcsCranePi*.5f)difference-=StorylandLcsCranePi;
    const float limit=.0035f*step;
    if(std::fabs(difference)<=limit)return true;
    const float angle=difference<0?-limit:limit;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    for(size_t column:{size_t(0),size_t(4),size_t(8)}) {
        const float x=vehicle.matrix[column],y=vehicle.matrix[column+1];
        vehicle.matrix[column]=x*cosine-y*sine;
        vehicle.matrix[column+1]=x*sine+y*cosine;
    }
    return false;
}

inline bool storylandAdvanceLcsCranes(StorylandLcsCraneSystem& system,uint32_t elapsedMilliseconds,
    uint32_t gameMilliseconds,const std::array<float,3>& camera,const std::array<float,3>& player,
    std::string& error) {
    if(!system.initialized||elapsedMilliseconds==0)return true;
    if(!storylandValidLcsCraneSystem(system)) {error="Saved native crane state is invalid.";return false;}
    if(!std::all_of(camera.begin(),camera.end(),[](float value){return std::isfinite(value);})||
        !std::all_of(player.begin(),player.end(),[](float value){return std::isfinite(value);})) {
        error="Crane camera and player positions must be finite.";return false;
    }
    const bool hasMilitary=std::any_of(system.cranes.begin(),system.cranes.end(),[](const auto& crane){return crane.military;});
    if(!storylandLcsCraneBindingsReady(system,hasMilitary,error))return false;
    const float step=std::min(3.0f,float(elapsedMilliseconds)*.05f);
    for(auto it=system.notifiedVehicles.begin();it!=system.notifiedVehicles.end();) {
        StorylandLcsCraneVehicle vehicle;
        std::string vehicleError;
        if(system.bindings.readVehicle(*it,vehicle,vehicleError))++it;
        else if(vehicleError.empty())it=system.notifiedVehicles.erase(it);
        else {error=vehicleError;return false;}
    }
    for(auto& stored:system.cranes) {
        if(!stored.top&&!stored.crusher&&(std::fabs(camera[0]-stored.originalMatrix[12])>=300||
            std::fabs(camera[1]-stored.originalMatrix[13])>=300))continue;
        auto crane=stored;
        StorylandLcsCraneVehicle vehicle;
        enum VehicleAction { KeepVehicle, UpdateVehicle, RemoveVehicle };
        VehicleAction vehicleAction=KeepVehicle;
        bool carryVehicle=false;
        uint32_t collectedMilitaryBit=0;
        std::vector<std::pair<int32_t,std::string>> notifications;
        bool hasVehicle=false;
        if(crane.vehicle>=0) {
            std::string vehicleError;
            hasVehicle=system.bindings.readVehicle(crane.vehicle,vehicle,vehicleError);
            if(!hasVehicle&&!vehicleError.empty()) {error=vehicleError;return false;}
            if(hasVehicle&&!storylandLcsCraneValidVehicle(vehicle)) {error="Native crane vehicle state is invalid.";return false;}
            if(!hasVehicle)crane.vehicle=-1;
        }
        const bool nearby=std::fabs(camera[0]-crane.originalMatrix[12])<150&&
            std::fabs(camera[1]-crane.originalMatrix[13])<150;
        if(((crane.status==StorylandLcsCrane::Activated||crane.status==StorylandLcsCrane::Deactivated)&&nearby)||
            crane.state!=StorylandLcsCrane::Idle) {
            switch(crane.state) {
            case StorylandLcsCrane::Idle:
                if(storylandLcsCraneMoveTo(crane,crane.pickupAngle,crane.pickupDistance,
                    crane.pickupHeight+4+(crane.crusher?4.5f:0),step)&&gameMilliseconds>crane.nextCheck) {
                    std::vector<StorylandLcsCraneVehicle> candidates;
                    if(!system.bindings.findVehicles(crane.pickup,candidates,error))return false;
                    for(const auto& candidate:candidates) {
                        if(!storylandLcsCraneValidVehicle(candidate)) {error="Native crane scan returned an invalid vehicle.";return false;}
                        if(!storylandLcsCraneInside(crane,candidate)||candidate.hasDriver||!candidate.automobile||
                            candidate.wrecked||candidate.health<250||
                            std::any_of(candidate.velocity.begin(),candidate.velocity.end(),
                                [](float speed){return std::fabs(speed)>=.01f;}))continue;
                        const bool allowed=storylandLcsCraneAllowsModel(crane,candidate.model);
                        const bool collected=crane.military&&(system.militaryCars&storylandLcsMilitaryCraneBit(candidate.model));
                        if(!allowed||collected) {
                            if(!system.notifiedVehicles.count(candidate.handle)&&
                                std::none_of(notifications.begin(),notifications.end(),[&](const auto& item) {
                                    return item.first==candidate.handle;
                                }))notifications.emplace_back(candidate.handle,
                                    !crane.military?"CR_1":allowed?"GA_20":"GA_19");
                            continue;
                        }
                        crane.vehicle=candidate.handle;crane.state=StorylandLcsCrane::Approaching;
                        vehicle=candidate;hasVehicle=true;
                    }
                }
                break;
            case StorylandLcsCrane::Approaching:
                if(!hasVehicle||!storylandLcsCraneInside(crane,vehicle)||vehicle.hasDriver||vehicle.beingEntered||
                    std::any_of(vehicle.velocity.begin(),vehicle.velocity.end(),[](float speed){return std::fabs(speed)>.01f;})) {
                    crane.vehicle=-1;crane.state=StorylandLcsCrane::Idle;
                } else {
                    const float x=vehicle.matrix[12]-crane.originalMatrix[12],y=vehicle.matrix[13]-crane.originalMatrix[13];
                    if(storylandLcsCraneMoveTo(crane,storylandLcsCraneAngle(x,y),std::hypot(x,y),
                        vehicle.matrix[14]+vehicle.roofHeight,step)) {
                        const float dx=vehicle.matrix[12]-crane.hookPosition[0],dy=vehicle.matrix[13]-crane.hookPosition[1];
                        const float dz=vehicle.matrix[14]+vehicle.roofHeight-crane.hookPosition[2];
                        if(dx*dx+dy*dy+dz*dz<.25f) {
                            crane.state=StorylandLcsCrane::Raising;
                            crane.hookVelocity[0]*=.4f;crane.hookVelocity[1]*=.4f;
                        }
                    }
                }
                break;
            case StorylandLcsCrane::Lifting:
            case StorylandLcsCrane::Raising:
            case StorylandLcsCrane::Aligning:
                if(!hasVehicle||vehicle.hasDriver) {
                    if(hasVehicle)vehicleAction=UpdateVehicle;
                    crane.vehicle=-1;crane.state=StorylandLcsCrane::Idle;
                    break;
                } else {
                    const bool rotated=storylandLcsCraneRotateVehicle(crane,vehicle,step);
                    if(crane.state==StorylandLcsCrane::Raising) {
                        if(storylandLcsCraneMove(crane.hookHeight,crane.pickupHeight+(crane.crusher?7.0f:4.0f),.3f*step*.06f))
                            crane.state=StorylandLcsCrane::Lifting;
                    } else if(crane.state==StorylandLcsCrane::Lifting) {
                        if(storylandLcsCraneMoveTo(crane,crane.dropoffAngle,crane.dropoffDistance,
                            crane.dropoffHeight+(crane.crusher?5.0f:2.0f),step,.3f))crane.state=StorylandLcsCrane::Aligning;
                    } else {
                        const bool moved=storylandLcsCraneMoveTo(crane,crane.dropoffAngle,crane.dropoffDistance,
                            crane.dropoffHeight,step,.3f);
                        const float distance=std::hypot(player[0]-vehicle.matrix[12],player[1]-vehicle.matrix[13]);
                        if(moved&&rotated&&(distance>5||!crane.wasMilitary)) {
                            if(crane.military) {
                                vehicleAction=RemoveVehicle;
                                collectedMilitaryBit=storylandLcsMilitaryCraneBit(vehicle.model);
                            } else vehicleAction=UpdateVehicle;
                            ++crane.collected;crane.vehicle=-1;hasVehicle=false;crane.state=StorylandLcsCrane::Dropping;
                        }
                    }
                }
                break;
            case StorylandLcsCrane::Dropping:
                if(storylandLcsCraneMoveTo(crane,crane.dropoffAngle,crane.dropoffDistance,
                    crane.dropoffHeight+(crane.crusher?7.0f:2.0f),step,.3f)) {
                    crane.state=StorylandLcsCrane::Idle;crane.nextCheck=gameMilliseconds+10000u;
                }
                break;
            }
            const auto hook=storylandLcsCraneHook(crane);
            for(size_t axis=0;axis<2;++axis) {
                crane.hookVelocity[axis]+=(hook[axis]-crane.hookPosition[axis])*step*.001f;
                crane.hookVelocity[axis]*=std::pow(.98f,step);
                crane.hookPosition[axis]+=crane.hookVelocity[axis]*step;
            }
            crane.hookPosition[2]=hook[2];
            if(hasVehicle&&(crane.state==StorylandLcsCrane::Lifting||crane.state==StorylandLcsCrane::Raising||
                crane.state==StorylandLcsCrane::Aligning)) {
                auto& matrix=vehicle.matrix;
                matrix[12]=crane.hookPosition[0];matrix[13]=crane.hookPosition[1];
                matrix[14]=crane.hookPosition[2]-vehicle.roofHeight;
                std::array<float,3> up{hook[0]-crane.hookPosition[0],hook[1]-crane.hookPosition[1],20};
                const float length=std::sqrt(up[0]*up[0]+up[1]*up[1]+up[2]*up[2]);
                for(auto& value:up)value/=length;
                const std::array<float,3> right{matrix[5]*up[2]-matrix[6]*up[1],
                    matrix[6]*up[0]-matrix[4]*up[2],matrix[4]*up[1]-matrix[5]*up[0]};
                const std::array<float,3> forward{up[1]*right[2]-up[2]*right[1],
                    up[2]*right[0]-up[0]*right[2],up[0]*right[1]-up[1]*right[0]};
                for(size_t axis=0;axis<3;++axis) {matrix[axis]=right[axis];matrix[4+axis]=forward[axis];matrix[8+axis]=up[axis];}
                if(!storylandLcsCraneFiniteMatrix(matrix)) {error="Native crane produced an invalid vehicle frame.";return false;}
                vehicleAction=UpdateVehicle;carryVehicle=true;
            }
        } else {
            const uint16_t phase=uint16_t((crane.randomSeed+(gameMilliseconds>>11))&15);
            if(phase<4)crane.hookAngle-=.002f*step;
            else if(phase>5&&phase<12)crane.hookAngle+=.002f*step;
            if(crane.hookAngle<0)crane.hookAngle+=2*StorylandLcsCranePi;
            else if(crane.hookAngle>2*StorylandLcsCranePi)crane.hookAngle-=2*StorylandLcsCranePi;
            crane.hookPosition=storylandLcsCraneHook(crane);crane.hookVelocity={};
        }
        const auto finite=[](const auto& values) {
            return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
        };
        if(!finite(crane.hookPosition)||!finite(crane.hookVelocity)) {
            error="Native crane hook movement exceeded finite coordinates.";return false;
        }
        if(!system.bindings.transform(crane,storylandLcsCraneFrame(crane),error))return false;
        const bool vehicleUpdated=vehicleAction==KeepVehicle||
            (vehicleAction==UpdateVehicle?system.bindings.updateVehicle(vehicle.handle,vehicle.matrix,
                carryVehicle,crane.crusher,error):system.bindings.removeVehicle(vehicle.handle,error));
        if(!vehicleUpdated) {
            // The renderer must accept the crane frame before a car can become owned or consumed.
            // Vehicle bindings validate before mutation; restore the crane if that validation fails.
            std::string restoreError;
            if(!system.bindings.transform(stored,storylandLcsCraneFrame(stored),restoreError))
                error+=" Crane transform rollback failed: "+restoreError;
            return false;
        }
        stored=std::move(crane);
        if(collectedMilitaryBit) {
            system.militaryCars|=collectedMilitaryBit;
            if((system.militaryCars&0x7F)!=0x7F) {
                system.bindings.reward(1500);system.bindings.message("GA_10",1500);
            }
        }
        for(const auto& notification:notifications) {
            system.notifiedVehicles.insert(notification.first);
            system.bindings.message(notification.second,-1);
        }
    }
    error.clear();return true;
}
