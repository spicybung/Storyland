#pragma once
#include "src/storyland/storyland_script_corona.h"
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/re/reVCS/world/storyland_vcs_object_water.h"
#include "src/re/reVCS/world/storyland_vcs_object_rotation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

inline std::array<float,16> storylandScriptObjectMatrix(float x,float y,float z,
    float rotationX,float rotationY,float heading) {
    constexpr float radians=0.017453292519943295f;
    const float cx=std::cos(rotationX*radians),sx=std::sin(rotationX*radians);
    const float cy=std::cos(rotationY*radians),sy=std::sin(rotationY*radians);
    const float cz=std::cos(heading*radians),sz=std::sin(heading*radians);
    return {cz*cy-sz*sx*sy,cz*sx*sy+sz*cy,-cx*sy,0,
        -sz*cx,cz*cx,sx,0,sz*sx*cy+cz*sy,sz*sy-cz*sx*cy,cx*cy,0,x,y,z,1};
}
struct StorylandScriptObjectBox {
    StorylandNativePoint minimum,maximum,position;
    float heading=0,rotationX=0,rotationY=0;
};

// Kinematic script props (including the MAIN.SCM gates) use the same authored
// matrix for collision and drawing. Read live state so sliding, deletion,
// collision disabling and area changes take effect without a second transform.
inline bool storylandScriptKinematicCollision(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Object& object,StorylandNativeCollisionInstance& instance) {
    if(!object.collisionEnabled||object.dynamic||object.area!=world.area||object.model<0||
        object.attachedCharacter>=0||object.attachedVehicle>=0)return false;
    instance.modelId=uint32_t(object.model);
    instance.poolHeader=0x2C;
    instance.matrix=storylandScriptObjectMatrix(object.x,object.y,object.z,
        object.rotationX,object.rotationY,object.heading);
    return true;
}
inline bool storylandScriptBoxesOverlap(const StorylandScriptObjectBox& first,
    const StorylandScriptObjectBox& second) {
    using Vector=std::array<float,3>;
    const auto basis=[](const StorylandScriptObjectBox& box) {
        const auto matrix=storylandScriptObjectMatrix(0,0,0,box.rotationX,box.rotationY,box.heading);
        return std::array<Vector,3>{{{matrix[0],matrix[1],matrix[2]},
            {matrix[4],matrix[5],matrix[6]},{matrix[8],matrix[9],matrix[10]}}};
    };
    const auto a=basis(first),b=basis(second);
    const auto corners=[](const StorylandScriptObjectBox& box,const std::array<Vector,3>& axes) {
        std::array<Vector,8> points{};
        for(size_t i=0;i<8;++i) {
            const Vector p{i&1?box.maximum.x:box.minimum.x,i&2?box.maximum.y:box.minimum.y,i&4?box.maximum.z:box.minimum.z};
            for(size_t j=0;j<3;++j)points[i][j]=axes[0][j]*p[0]+axes[1][j]*p[1]+axes[2][j]*p[2];
            points[i][0]+=box.position.x;points[i][1]+=box.position.y;points[i][2]+=box.position.z;
        }
        return points;
    };
    const auto firstCorners=corners(first,a),secondCorners=corners(second,b);
    const auto separated=[&](const Vector& axis) {
        if(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]<1e-12f)return false;
        float lowA=INFINITY,highA=-INFINITY,lowB=INFINITY,highB=-INFINITY;
        for(size_t i=0;i<8;++i) {
            float pa=0,pb=0;
            for(size_t j=0;j<3;++j) {pa+=firstCorners[i][j]*axis[j];pb+=secondCorners[i][j]*axis[j];}
            lowA=std::min(lowA,pa);highA=std::max(highA,pa);lowB=std::min(lowB,pb);highB=std::max(highB,pb);
        }
        return highA<=lowB||highB<=lowA;
    };
    for(const auto& axis:a)if(separated(axis))return false;
    for(const auto& axis:b)if(separated(axis))return false;
    for(const auto& x:a)for(const auto& y:b)
        if(separated({x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]}))return false;
    return true;
}

inline bool storylandReadObjectProperties(const std::vector<uint8_t>& bytes,int32_t model,
    StorylandMissionEnvironment::Object::DamageState& result,bool vcs=true) {
    const auto word=[&](size_t offset) {
        uint32_t value=0;if(offset<=bytes.size()&&bytes.size()-offset>=4)std::memcpy(&value,bytes.data()+offset,4);
        return value;
    };
    if(bytes.size()<0x78||model<0||uint32_t(model)>=word(0x38))return false;
    const uint32_t table=word(0x3C),info=word(size_t(table)+size_t(model)*4),rows=word(0x74);
    if(!table||!info||uint64_t(info)+0x1C>bytes.size()||!rows)return false;
    int16_t index=-1;std::memcpy(&index,bytes.data()+info+0x1A,2);
    const size_t stride=vcs?36:32;
    if(index<0||uint64_t(rows)+uint64_t(index)*stride+stride>bytes.size())return false;
    const size_t row=size_t(rows)+size_t(index)*stride;
    auto candidate=result;
    const auto real=[&](size_t at) {float f=0;std::memcpy(&f,bytes.data()+row+at,4);return f;};
    candidate.mass=real(0);candidate.turnMass=real(4);candidate.airResistance=real(8);
    candidate.elasticity=real(12);candidate.buoyancy=real(16);candidate.uprootLimit=real(20);
    candidate.multiplier=real(24);candidate.damageEffect=bytes[row+28];candidate.collisionResponse=bytes[row+29];
    candidate.cameraAvoid=(bytes[row+30]&1)!=0;
    candidate.effect1=vcs?bytes[row+31]:0;candidate.effect2=vcs?bytes[row+32]:0;
    for(float value:{candidate.mass,candidate.turnMass,candidate.airResistance,candidate.elasticity,
        candidate.buoyancy,candidate.uprootLimit,candidate.multiplier})if(!std::isfinite(value))return false;
    if(candidate.mass<=0||candidate.turnMass<=0||candidate.airResistance<0||candidate.airResistance>1)return false;
    candidate.disableMoveForce=candidate.mass>=(vcs?99999.0f:99998.0f);
    candidate.disableTurnForce=candidate.disableMoveForce;candidate.propertiesLoaded=true;
    result=candidate;return true;
}
inline bool storylandObjectIsBreakableGlass(const std::vector<uint8_t>& bytes,int32_t model) {
    const auto word=[&](size_t o){uint32_t v=0;if(o<=bytes.size()&&bytes.size()-o>=4)std::memcpy(&v,bytes.data()+o,4);return v;};
    if(bytes.size()<0x40||model<0||uint32_t(model)>=word(0x38))return false;
    const uint32_t table=word(0x3C),info=word(size_t(table)+size_t(model)*4);
    if(!info||uint64_t(info)+0x3C>bytes.size()||(bytes[info+0x10]!=1&&bytes[info+0x10]!=3))return false;
    uint16_t flags=0;std::memcpy(&flags,bytes.data()+info+0x3A,2);return (flags&0x6000)!=0;
}
inline void storylandHydrateScriptObject(StorylandMissionEnvironment& world,StorylandMissionEnvironment::Object& object) {
    if(object.model<0&&world.resolveObjectModel) {
        const int32_t native=world.resolveObjectModel(object.model);if(native>=0)object.model=native;
    }
    if(object.damage.propertiesLoaded||!world.nativeObjectProperties)return;
    auto properties=object.damage;
    if(world.nativeObjectProperties(object.model,properties)) {
        object.damage=properties;object.gravity=!properties.disableMoveForce;
    }
}
inline int32_t storylandPrepareObjectPoolIndices(StorylandMissionEnvironment& world) {
    std::set<int32_t> used;
    for (const auto& object : world.objects) {
        if (object.nativePoolIndex >= 0) {
            used.insert(object.nativePoolIndex);
        }
    }
    int32_t free = 0;
    for (auto& object : world.objects) {
        if (object.nativePoolIndex >= 0) {
            continue;
        }
        while (used.count(free)) {
            ++free;
        }
        object.nativePoolIndex = free;
        used.insert(free);
    }
    free = 0;
    while (used.count(free)) {
        ++free;
    }
    return free;
}

inline bool storylandPrepareScriptObject(StorylandMissionEnvironment& world,
    StorylandMissionEnvironment::Object& object,bool baseOffset,std::string& error) {
    for(float coordinate:{object.x,object.y,object.z})if(!std::isfinite(coordinate)) {
        error="Object creation requires finite coordinates.";return false;
    }
    storylandHydrateScriptObject(world,object);
    if(object.z==-100.0f||object.z<=-250.0f) {
        float ground=0;
        if(!world.groundHeightAt||!world.groundHeightAt({object.x,object.y,1000},ground)||!std::isfinite(ground)) {
            error="Object ground sentinel requires the native collision world.";return false;
        }
        object.z=ground;
    }
    if(baseOffset) {
        if(!world.objectWorld) {error="CREATE_OBJECT requires its native COL base offset.";return false;}
        const auto model=world.objectWorld->collisionModels.find(uint32_t(object.model));
        if(model==world.objectWorld->collisionModels.end()||!std::isfinite(model->second.minimum.z)) {
            error="CREATE_OBJECT has no decoded native COL model.";return false;
        }
        object.z-=model->second.minimum.z;
        if(!std::isfinite(object.z)) {error="Object position exceeds the finite world range.";return false;}
    }
    if (object.nativePoolIndex < 0) {
        object.nativePoolIndex = storylandPrepareObjectPoolIndices(world);
    }
    return true;
}
inline void storylandDeleteScriptObject(StorylandMissionEnvironment& world,int32_t handle) {
    world.releaseVcsObjectAttachments(handle);
    world.lcsExtendedWorld.brokenGlassObjects.erase(handle);
    world.lcsExtendedWorld.crackedGlassObjects.erase(handle);
    world.missionCleanupObjects.erase(handle);
    world.objects.erase(std::remove_if(world.objects.begin(),world.objects.end(),
        [&](const auto& object){return object.handle==handle;}),world.objects.end());
    using Kind=StorylandMissionEnvironment::TargetKind;
    if(world.playerTarget.kind==Kind::Object&&world.playerTarget.handle==handle)world.playerTarget={};
    for(auto& entry:world.characterTargets)
        if(entry.second.kind==Kind::Object&&entry.second.handle==handle)entry.second={};
    const auto clearSource=[&](StorylandMissionEnvironment::DamageSource& source) {
        if(source.kind==StorylandMissionEnvironment::DamageSourceKind::Object&&source.handle==handle)source={};
    };
    clearSource(world.playerDamageSource);
    for(auto& entry:world.missionCharacters)clearSource(entry.second.lastDamageSource);
    for(auto& entry:world.missionVehicles)clearSource(entry.second.lastDamageSource);
    for(auto& explosion:world.scriptExplosions)clearSource(explosion.source);
    for(auto& entry:world.scriptFires)clearSource(entry.second.source);
    // The four raw native target registration handles intentionally survive deletion.
    world.radarBlips.erase(std::remove_if(world.radarBlips.begin(),world.radarBlips.end(),
        [&](const auto& blip){return blip.entity==StorylandMissionEnvironment::BlipEntity::Object&&blip.entityHandle==handle;}),world.radarBlips.end());
}
inline bool storylandObjectWeaponMatches(int32_t last,int32_t requested) {
    if(last<0)return false;
    if(requested==49)return last<=11;
    if(requested==50)return last<=39||last==44||last==45;
    return last==requested;
}
inline float storylandObjectEeProduct(float first,float second) {
    if(std::fpclassify(first)==FP_SUBNORMAL)first=std::copysign(0.0f,first);
    if(std::fpclassify(second)==FP_SUBNORMAL)second=std::copysign(0.0f,second);
    const double product=double(first)*double(second);
    const double maximum=std::numeric_limits<float>::max();
    if(product>=maximum)return std::numeric_limits<float>::max();
    if(product<=-maximum)return -std::numeric_limits<float>::max();
    float value=float(product);
    if(std::fabs(double(value))>std::fabs(product))value=std::nextafter(value,0.0f);
    return std::fpclassify(value)==FP_SUBNORMAL?std::copysign(0.0f,value):value;
}
inline void storylandHideDamagedObject(StorylandMissionEnvironment::Object& object) {
    object.damage.visible=false;object.collisionEnabled=false;object.dynamic=false;
    object.damage.proofs|=4;object.velocity={};object.rotationVelocity={};
}
enum class StorylandObjectDamageResult { None, Changed, Delete };
inline StorylandObjectDamageResult storylandDamageScriptObject(StorylandMissionEnvironment::Object& object,float damage) {
    if(!std::isfinite(damage)||!std::isfinite(object.damage.multiplier))return StorylandObjectDamageResult::None;
    auto& state=object.damage;
    const float scaled=storylandObjectEeProduct(damage,state.multiplier);
    if(state.health>0) {
        const float capped=std::min(127.0f,scaled);
        const int32_t integer=capped<float(INT32_MIN)||capped>=2147483648.0f?INT32_MIN:int32_t(capped);
        const int8_t amount=int8_t(uint8_t(integer));
        state.health=int8_t(uint8_t(int32_t(state.health)-int32_t(amount)));
        if(state.health<=0) {
            if(state.damageEffect==1||state.damageEffect==4) {
                state.renderDamaged=true;state.health=0;object.collisionEnabled=false;
            }
            else {storylandHideDamagedObject(object);return StorylandObjectDamageResult::Delete;}
        }
        return StorylandObjectDamageResult::Changed;
    }
    if(!state.damageEffect||!object.collisionEnabled||!(scaled>150.0f))return StorylandObjectDamageResult::None;
    switch(state.damageEffect) {
    case 1:state.renderDamaged=true;break;
    case 3:storylandHideDamagedObject(object);break;
    case 4:if(state.renderDamaged)storylandHideDamagedObject(object);else state.renderDamaged=true;break;
    default:return StorylandObjectDamageResult::None;
    }
    return StorylandObjectDamageResult::Changed;
}

// Test the decoded COL primitives in a rigid native world frame. Bounds
// reject distant rays; they must not fill holes between authored triangles.
inline bool storylandTraceCollisionModel(const StorylandNativeCollisionModel& col,
    const std::array<float,16>& matrix,const StorylandNativePoint& start,
    const StorylandNativePoint& end,float& fraction,StorylandNativePoint& normal) {
    if(!std::isfinite(fraction)||fraction<=0)return false;
    for(float value:{start.x,start.y,start.z,end.x,end.y,end.z})if(!std::isfinite(value))return false;
    if(!std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))return false;
    const auto local=[&](const StorylandNativePoint& point) {
        const std::array<float,3> delta{point.x-matrix[12],point.y-matrix[13],point.z-matrix[14]};
        std::array<float,3> result{};
        for(size_t axis=0;axis<3;++axis)for(size_t k=0;k<3;++k)result[axis]+=matrix[axis*4+k]*delta[k];
        return result;
    };
    using V=std::array<double,3>;
    const auto startLocal=local(start),endLocal=local(end);
    const V a{startLocal[0],startLocal[1],startLocal[2]},b{endLocal[0],endLocal[1],endLocal[2]};
    const auto sub=[](const V& x,const V& y)->V {return {x[0]-y[0],x[1]-y[1],x[2]-y[2]};};
    const auto dot=[](const V& x,const V& y) {return x[0]*y[0]+x[1]*y[1]+x[2]*y[2];};
    const auto cross=[](const V& x,const V& y)->V {return {x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]};};
    const auto point=[](const StorylandNativePoint& p)->V {return {p.x,p.y,p.z};};
    const V direction=sub(b,a);const double lengthSquared=dot(direction,direction);
    if(lengthSquared<1e-20)return false;
    double nearest=std::min(1.0,double(fraction));V hitNormal{};bool hit=false;
    const auto box=[&](const StorylandNativePoint& minimum,const StorylandNativePoint& maximum,
        double& entry,V& n) {
        const V lo=point(minimum),hi=point(maximum);
        double enter=0,leave=1;n={-direction[0],-direction[1],-direction[2]};
        for(size_t axis=0;axis<3;++axis) {
            if(std::fabs(direction[axis])<1e-12) {if(a[axis]<lo[axis]||a[axis]>hi[axis])return false;continue;}
            double nearDistance=(lo[axis]-a[axis])/direction[axis];
            double farDistance=(hi[axis]-a[axis])/direction[axis];
            const double sign=direction[axis]>0?-1.0:1.0;
            if(nearDistance>farDistance)std::swap(nearDistance,farDistance);
            if(nearDistance>enter) {enter=nearDistance;n={};n[axis]=sign;}
            leave=std::min(leave,farDistance);if(enter>leave)return false;
        }
        entry=enter;return enter>=0&&enter<=1;
    };
    double boundsEntry=0;V boundsNormal{};
    if(!box(col.minimum,col.maximum,boundsEntry,boundsNormal)||boundsEntry>=nearest)return false;
    const auto record=[&](double entry,V n) {
        if(entry<0||entry>=nearest||!std::isfinite(entry))return;
        const double length=std::sqrt(dot(n,n));if(!(length>1e-12))return;
        for(auto& component:n)component/=length;
        if(dot(n,direction)>0)for(auto& component:n)component=-component;
        nearest=entry;hitNormal=n;hit=true;
    };
    for(const auto& primitive:col.boxes) {
        double entry=0;V n{};if(box(primitive.minimum,primitive.maximum,entry,n))record(entry,n);
    }
    for(const auto& sphere:col.spheres) {
        if(!(sphere.radius>0))continue;
        const V offset=sub(a,point(sphere.center));const double projection=dot(offset,direction);
        const double c=dot(offset,offset)-double(sphere.radius)*sphere.radius;
        const double discriminant=projection*projection-lengthSquared*c;if(discriminant<0)continue;
        const double entry=c<=0?0:(-projection-std::sqrt(discriminant))/lengthSquared;
        V n{offset[0]+entry*direction[0],offset[1]+entry*direction[1],offset[2]+entry*direction[2]};
        if(dot(n,n)<1e-20)n={-direction[0],-direction[1],-direction[2]};record(entry,n);
    }
    for(const auto& triangle:col.triangles) {
        if(triangle.vertices[0]>=col.vertices.size()||triangle.vertices[1]>=col.vertices.size()||triangle.vertices[2]>=col.vertices.size())continue;
        const V v0=point(col.vertices[triangle.vertices[0]]),edge1=sub(point(col.vertices[triangle.vertices[1]]),v0),
            edge2=sub(point(col.vertices[triangle.vertices[2]]),v0),p=cross(direction,edge2);
        const double determinant=dot(edge1,p);if(std::fabs(determinant)<1e-12)continue;
        const V offset=sub(a,v0);const double u=dot(offset,p)/determinant;if(u<0||u>1)continue;
        const V q=cross(offset,edge1);const double v=dot(direction,q)/determinant;if(v<0||u+v>1)continue;
        record(dot(edge2,q)/determinant,cross(edge1,edge2));
    }
    if(!hit)return false;
    fraction=float(nearest);
    normal={float(matrix[0]*hitNormal[0]+matrix[4]*hitNormal[1]+matrix[8]*hitNormal[2]),
        float(matrix[1]*hitNormal[0]+matrix[5]*hitNormal[1]+matrix[9]*hitNormal[2]),
        float(matrix[2]*hitNormal[0]+matrix[6]*hitNormal[1]+matrix[10]*hitNormal[2])};
    return true;
}
inline bool storylandTraceScriptObject(const StorylandNativeWorld& world,
    const StorylandMissionEnvironment::Object& object,const StorylandNativePoint& start,
    const StorylandNativePoint& end,float& fraction,StorylandNativePoint& normal) {
    if(!object.collisionEnabled||!object.damage.visible)return false;
    const auto found=world.collisionModels.find(uint32_t(object.model));
    if(found==world.collisionModels.end())return false;
    return storylandTraceCollisionModel(found->second,storylandScriptObjectMatrix(
        object.x,object.y,object.z,object.rotationX,object.rotationY,object.heading),
        start,end,fraction,normal);
}
inline bool storylandApplyScriptObjectAttachment(StorylandMissionEnvironment::Object& object,
    const std::array<float,16>& parent,const std::array<float,3>& offset,const std::array<float,3>& rotation) {
    if(!std::all_of(parent.begin(),parent.end(),[](float v){return std::isfinite(v);}))return false;
    const auto local=storylandScriptObjectMatrix(0,0,0,rotation[0],rotation[1],rotation[2]);
    std::array<float,16> matrix{};matrix[15]=1;
    for(size_t column=0;column<3;++column)for(size_t row=0;row<3;++row)
        for(size_t k=0;k<3;++k)matrix[column*4+row]+=parent[k*4+row]*local[column*4+k];
    for(size_t row=0;row<3;++row) {
        matrix[12+row]=parent[12+row];
        for(size_t k=0;k<3;++k)matrix[12+row]+=parent[k*4+row]*offset[k];
    }
    if(!std::all_of(matrix.begin(),matrix.end(),[](float v){return std::isfinite(v);}))return false;
    constexpr float degrees=57.29577951308232f;
    const float pitch=std::asin(std::clamp(matrix[6],-1.0f,1.0f));
    // Invert the same Z-X-Y basis used by rendering and native object rotation.
    const bool regular=std::fabs(std::cos(pitch))>1e-5f;
    object.rotationX=pitch*degrees;
    object.rotationY=regular?std::atan2(-matrix[2],matrix[10])*degrees:0;
    object.heading=(regular?std::atan2(-matrix[4],matrix[5]):std::atan2(matrix[1],matrix[0]))*degrees;
    object.x=matrix[12];object.y=matrix[13];object.z=matrix[14];return true;
}
struct StorylandExplosionProfile {
    float radius=0,force=0;
    uint32_t lifetime=750;
    int32_t weapon=44;
};
inline bool storylandExplosionProfile(int32_t type,StorylandExplosionProfile& result) {
    // CExplosion::AddExplosion jump table0x4B4180, data paths0x2E1394..2E21D8.
    static constexpr StorylandExplosionProfile profiles[]={
        {9,300,750,12},{6,0,3000,17},{10,300,750,32},
        {9,300,4250,44},{9,300,4250,44},{9,300,4250,44},
        {6,300,750,44},{12,500,750,44},{10,150,750,15},
        {7,150,750,44},{10,150,750,44},{8,50,750,44}};
    if(type<0||size_t(type)>=std::size(profiles))return false;
    result=profiles[type];return true;
}
inline bool storylandAddScriptExplosion(StorylandMissionEnvironment& world,int32_t type,
    const std::array<float,3>& position,std::string& error,StorylandMissionEnvironment::DamageSource source={}) {
    StorylandExplosionProfile profile;
    if(!storylandExplosionProfile(type,profile)||
        !std::all_of(position.begin(),position.end(),[](float v){return std::isfinite(v);})) {
        error="ADD_EXPLOSION requires a native type0..11 and finite coordinates.";return false;
    }
    auto slot=std::find_if(world.scriptExplosions.begin(),world.scriptExplosions.end(),
        [&](const auto& e){return !e.active||uint32_t(world.gameMilliseconds-e.startedAt)>=e.lifetime;});
    if(slot==world.scriptExplosions.end())return true; // Retail ignores allocation failure.
    if(!world.createScriptExplosion) {error="ADD_EXPLOSION requires the live world-effect consumer.";return false;}
    StorylandMissionEnvironment::ScriptExplosion candidate{position,type,world.gameMilliseconds,profile.lifetime,true};
    candidate.source=source;
    if(!world.createScriptExplosion(candidate,error))return false;
    *slot=candidate;return true;
}
inline void storylandAdvanceScriptObjects(StorylandMissionEnvironment& world,uint32_t elapsed,bool vcs=true) {
    world.frameScriptCoronas.clear();
    const float nativeStep=std::isfinite(world.nativeScriptTimeStep)?std::max(0.0f,world.nativeScriptTimeStep):0;
    for(auto& explosion:world.scriptExplosions)
        if(explosion.active&&uint32_t(world.gameMilliseconds-explosion.startedAt)>=explosion.lifetime)explosion.active=false;
    for(auto& object:world.objects)if(object.recordsCollisions) {
        object.collided=false;
        object.collidedVehicles.clear();
    }
    for(auto& object:world.objects) {
        storylandHydrateScriptObject(world,object);
        if(object.recordsCollisions) {
            if(object.collisionEnabled&&world.objectSlideCollision&&
                world.objectSlideCollision(object.handle,{object.x,object.y,object.z})>0)object.collided=true;
        }
        if(object.attachedCharacter>=0) {
            if(object.attachedCharacter==(vcs?0:1)||world.missionCharacters.count(object.attachedCharacter)) {
                std::array<float,16> matrix;
                if(world.characterBoneMatrix&&world.characterBoneMatrix(object.attachedCharacter,object.attachedBone,matrix))
                    storylandApplyScriptObjectAttachment(object,matrix,object.attachmentOffset,object.attachmentRotation);
                continue;
            }
            object.attachedCharacter=-1;
        }
        if(object.attachedVehicle>=0) {
            const auto car=world.missionVehicles.find(object.attachedVehicle);
            if(car!=world.missionVehicles.end()&&!car->second.deleted) {
                std::array<float,16> matrix;
                if(world.vehicleWorldMatrix&&world.vehicleWorldMatrix(object.attachedVehicle,matrix)) {
                    storylandApplyScriptObjectAttachment(object,matrix,object.attachmentOffset,object.attachmentRotation);
                    if(!vcs) {
                        std::array<float,3> velocity{};
                        if(world.vehicleWorldVelocity&&world.vehicleWorldVelocity(object.attachedVehicle,velocity)&&
                            std::all_of(velocity.begin(),velocity.end(),[](float value){return std::isfinite(value);}))
                            object.velocity=velocity;
                        if(std::all_of(car->second.angularVelocity.begin(),car->second.angularVelocity.end(),
                            [](float value){return std::isfinite(value);}))object.rotationVelocity=car->second.angularVelocity;
                    }
                }
                continue;
            }
            object.attachedVehicle=-1;
            if(!vcs)object.ignoredVehicle=-1;
        }
        if(!object.dynamic||object.positionFrozen||object.damage.disableMoveForce)continue;
        const float seconds=object.damage.propertiesLoaded?nativeStep/50.0f:std::min(.1f,float(elapsed)*.001f);
        if(seconds<=0)continue;
        if(object.gravity)object.velocity[2]-=20.0f*seconds;
        if(object.damage.propertiesLoaded) {
            const float resistance=object.damage.airResistance;
            const float speed=std::hypot(object.velocity[0],object.velocity[1],object.velocity[2])/50.0f;
            const float linear=std::pow(resistance>.1f?resistance:std::max(0.0f,1.0f-resistance*speed),nativeStep);
            const float angular=resistance>.1f?linear:.99f;
            for(auto& value:object.velocity)value*=linear;
            for(auto& value:object.rotationVelocity)value*=angular;
        }
        const float distance=std::hypot(object.velocity[0],object.velocity[1],object.velocity[2])*seconds;
        const int steps=std::clamp(int(std::ceil(std::min(distance,64.0f)/.25f)),1,256);
        const float dt=seconds/float(steps);
        for(int index=0;index<steps;++index) {
            std::array<float,3> next{object.x+object.velocity[0]*dt,object.y+object.velocity[1]*dt,object.z+object.velocity[2]*dt};
            bool blocked=object.collisionEnabled&&world.objectSlideCollision&&world.objectSlideCollision(object.handle,next)>0;
            if(object.collisionEnabled&&world.objectSegmentCollision&&world.objectSegmentCollision(
                {object.x,object.y,object.z},next)<1)blocked=true;
            if(blocked) {if(object.recordsCollisions)object.collided=true;for(auto& value:object.velocity)value*= -std::clamp(object.damage.elasticity,0.0f,1.0f);break;}
            object.x=next[0];object.y=next[1];object.z=next[2];
        }
        if(vcs&&!object.damage.disableTurnForce&&
            std::any_of(object.rotationVelocity.begin(),object.rotationVelocity.end(),[](float value){return value!=0;})) {
            const auto matrix=storylandScriptObjectMatrix(object.x,object.y,object.z,
                object.rotationX,object.rotationY,object.heading);
            std::array<float,16> advanced;
            const std::array<float,3> angular{object.rotationVelocity[0]/50.0f,
                object.rotationVelocity[1]/50.0f,object.rotationVelocity[2]/50.0f};
            if(storylandVcsAdvanceObjectFrame(matrix,angular,nativeStep,advanced)) {
                const float pitch=std::asin(std::clamp(advanced[6],-1.0f,1.0f));
                const bool regular=std::fabs(std::cos(pitch))>1e-5f;
                object.rotationX=pitch*57.295779513f;
                object.rotationY=regular?std::atan2(-advanced[2],advanced[10])*57.295779513f:0;
                object.heading=(regular?std::atan2(-advanced[4],advanced[5]):
                    std::atan2(advanced[1],advanced[0]))*57.295779513f;
            }
        } else if(!vcs&&!object.damage.disableTurnForce) {
            object.rotationX+=object.rotationVelocity[0]*seconds*57.295779513f;
            object.rotationY+=object.rotationVelocity[1]*seconds*57.295779513f;
            object.heading+=object.rotationVelocity[2]*seconds*57.295779513f;
        }
        if(vcs&&object.damage.propertiesLoaded&&world.objectWorld&&world.waterHeightAt) {
            const auto collision=world.objectWorld->collisionModels.find(uint32_t(object.model));
            const auto sampleWater=[&](float x,float y,float referenceZ) {
                StorylandVcsHovercraft::WaterSample sample;
                sample.available=world.waterHeightAt({x,y,referenceZ},sample.height)&&
                    storylandVcsWaterDepthAllowsContact(referenceZ,sample.height);
                return sample;
            };
            if(collision==world.objectWorld->collisionModels.end()||
                !storylandApplyVcsObjectWater(object,collision->second,
                    storylandScriptObjectMatrix(object.x,object.y,object.z,
                        object.rotationX,object.rotationY,object.heading),nativeStep,sampleWater))
                object.inWater=false;
        } else if(world.objectInWater)object.inWater=world.objectInWater(object);
    }
    for(auto& entry:world.scriptFires) {
        auto& fire=entry.second;if(!fire.active)continue;
        if(!fire.scriptOwned&&int32_t(world.gameMilliseconds-fire.endsAt)>=0) {fire.active=false;continue;}
        fire.strength=std::min(1.0f,fire.strength+.002f*nativeStep);
        if(!fire.attachedKind)continue;
        if(fire.attachedKind==1) {
            if(fire.attachedHandle==(vcs?0:1)) {fire.position={world.x,world.y,world.z};if(world.health<=0)fire.active=false;}
            else {
                const auto ped=world.missionCharacters.find(fire.attachedHandle);
                if(ped==world.missionCharacters.end()||!ped->second.alive)fire.active=false;
                else fire.position={ped->second.x,ped->second.y,ped->second.z};
            }
        } else {
            const auto car=world.missionVehicles.find(fire.attachedHandle);
            if(car==world.missionVehicles.end()||car->second.deleted)fire.active=false;
            else fire.position={car->second.x,car->second.y,car->second.z};
        }
    }
    for(auto entry=world.scriptFires.begin();entry!=world.scriptFires.end();)
        if(!entry->second.active&&!entry->second.scriptOwned)entry=world.scriptFires.erase(entry);else ++entry;
}

// VCS 132C50 selects object effect 8, whose 291ED0 producer emits a
// twelve-particle BALLOON_EXP ring and positional native one-shot event 0x27.
inline void storylandEmitVcsBalloonBurst(StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::Object& object) {
    std::array<float,3> eye{world.x,world.y,world.z};
    if(world.particleCameraPosition) {
        const auto camera=world.particleCameraPosition();
        if(std::all_of(camera.begin(),camera.end(),[](float value){return std::isfinite(value);}))eye=camera;
    }
    float dx=eye[0]-object.x,dy=eye[1]-object.y;
    if(dx*dx+dy*dy<.01f)dx=1.0f;
    const float length=std::hypot(dx,dy);
    if(length>0&&std::isfinite(length)) {dx/=length;dy/=length;}
    else {dx=1;dy=0;}
    std::vector<StorylandMissionEnvironment::ScriptParticle> particles;
    particles.reserve(12);
    constexpr float pi=3.14159265358979323846f;
    for(unsigned index=0;index<12;++index) {
        // Native truncates its floating-point index on a 1024-position turn.
        const unsigned angleIndex=unsigned(float(index)*(256.0f/3.0f))&1023u;
        const float angle=float(angleIndex)*(2.0f*pi/1024.0f);
        const float horizontal=.1f*std::sin(angle),vertical=.1f*std::cos(angle);
        StorylandMissionEnvironment::ScriptParticle particle;
        particle.type=79;particle.position={object.x,object.y,object.z};
        particle.velocity={-dy*horizontal,dx*horizontal,vertical};
        const float random=float(world.nextNativeRandom())*(1.0f/2147483648.0f);
        particle.size=1.5f+.5f*random;particle.lifetimeMilliseconds=1000;
        particles.push_back(particle);
    }
    if(world.createScriptParticles) {
        std::string error;
        world.createScriptParticles(particles,error);
    }
    if(world.audioSubsystemEnabled&&world.scriptSounds.size()<4096) {
        StorylandMissionEnvironment::ScriptSound sound;
        sound.id=39;sound.position={object.x,object.y,object.z};
        world.scriptSounds.push_back(sound);
    }
}

inline void storylandHitScriptObject(StorylandMissionEnvironment& world,int32_t handle,
    int32_t weapon,float damage,bool melee,bool shotgun,const StorylandNativePoint& normal) {
    const auto found=std::find_if(world.objects.begin(),world.objects.end(),[&](const auto& o){return o.handle==handle;});
    if(found==world.objects.end())return;
    auto& object=*found;storylandHydrateScriptObject(world,object);
    if(!melee&&world.objectWorld&&!world.objectWorld->zones.vcs&&
        world.nativeObjectBreakableGlass&&world.nativeObjectBreakableGlass(object.model)) {
        if(!object.collisionEnabled||(object.damage.proofs&1u))return;
        if(world.lcsExtendedWorld.crackedGlassObjects.insert(handle).second)return;
        if((world.nextNativeRandom()&3u)==2u) {
            world.lcsExtendedWorld.brokenGlassObjects.insert(handle);
            object.damage.visible=false;object.collisionEnabled=false;
        }
        return;
    }
    const bool damageBranch=object.damage.mass>=99999.0f||object.damage.multiplier>=99.9f;
    if(melee||shotgun)object.damage.lastWeapon=int8_t(uint8_t(weapon));
    if(damageBranch) {
        if(object.damage.proofs&(melee?16u:1u))return;
        object.damage.lastWeapon=int8_t(uint8_t(weapon));
        if(storylandDamageScriptObject(object,damage)==StorylandObjectDamageResult::Delete) {
            if(world.objectWorld&&world.objectWorld->zones.vcs&&object.damage.effect2==8)
                storylandEmitVcsBalloonBurst(world,object);
            storylandDeleteScriptObject(world,handle);
        }
    } else if(!object.damage.disableMoveForce&&object.damage.mass>0&&std::isfinite(object.damage.mass)) {
        // Native hit impulse is applied in native velocity units then limited
        // to +/-4 per component; the host stores world units per second.
        const float force=melee?-5.0f:-4.0f;
        const std::array<float,3> direction{normal.x,normal.y,normal.z};
        for(size_t axis=0;axis<3;++axis)
            object.velocity[axis]=std::clamp(object.velocity[axis]/50.0f+force*direction[axis]/object.damage.mass,-4.0f,4.0f)*50.0f;
        object.dynamic=true;
    }
}
inline void storylandExplodeScriptObjects(StorylandMissionEnvironment& world,
    const StorylandNativePoint& position,float radius,int32_t /*weapon*/,float force=300.0f) {
    if(!std::isfinite(radius)||radius<=0||!std::isfinite(force)||force<=0)return;
    std::vector<int32_t> removed;
    for(auto& object:world.objects) {
        storylandHydrateScriptObject(world,object);
        if(!object.damage.visible||(object.damage.proofs&4u))continue;
        const float distance=std::hypot(object.x-position.x,object.y-position.y,object.z-position.z);
        if(distance>=radius)continue;
        const float falloff=std::min(1.0f,2.0f*(1-distance/radius));
        // CWorld::TriggerExplosion (0x1929A8) does not overwrite CEntity+0x5B
        // for objects. Preserve a preceding bullet/melee damage query record.
        const bool glass=world.nativeObjectBreakableGlass&&world.nativeObjectBreakableGlass(object.model);
        if(glass&&distance<10) {
            object.damage.visible=false;object.collisionEnabled=false;
            world.lcsExtendedWorld.brokenGlassObjects.insert(object.handle);
            if(world.objectWorld&&!world.objectWorld->zones.vcs)continue;
        }
        const bool special=object.model==world.nativeExplosiveObjects[0]||object.model==world.nativeExplosiveObjects[1];
        if(!glass&&!object.dynamic&&((object.damage.uprootLimit>0&&object.damage.uprootLimit<force)||special))object.dynamic=true;
        const float damage=(object.dynamic?300.0f:3000.0f)*falloff;
        if(object.dynamic&&!object.positionFrozen&&!object.damage.disableMoveForce) {
            const float impulse=force*falloff*0.0007142857066355646f/std::max(distance,.01f)*50.0f;
            object.velocity[0]+=(object.x-position.x)*impulse;
            object.velocity[1]+=(object.y-position.y)*impulse;
            object.velocity[2]+=std::max(0.0f,object.z-position.z)*impulse;
        }
        if(storylandDamageScriptObject(object,damage)==StorylandObjectDamageResult::Delete) {
            if(world.objectWorld&&world.objectWorld->zones.vcs&&object.damage.effect2==8)
                storylandEmitVcsBalloonBurst(world,object);
            removed.push_back(object.handle);
        }
    }
    for(int32_t handle:removed)storylandDeleteScriptObject(world,handle);
}

inline float storylandNativeObjectMeleeDamage(const std::vector<uint8_t>& bytes,int32_t weapon,
    uint32_t sequence,bool moving) {
    if(bytes.size()<0xE8||weapon<0||weapon>63)return 0;
    const auto word=[&](size_t at) {uint32_t value=0;if(at<=bytes.size()&&bytes.size()-at>=4)std::memcpy(&value,bytes.data()+at,4);return value;};
    const uint32_t manager=word(0xE4);
    if(!manager||uint64_t(manager)+16>bytes.size())return 0;
    const uint32_t table=word(manager+8),count=word(manager+12);
    // Match the actual animations used by the host melee controller:307,306,
    //305 (native fight entries14,17,18),319 running (16),or armed214 (14).
    static constexpr uint32_t fists[]={14,17,18};
    const uint32_t entry=weapon==0?(moving?16u:fists[(sequence?sequence-1:0)%3]):14u;
    if(entry>=count||!table||uint64_t(table)+uint64_t(entry)*64+64>bytes.size())return 0;
    const size_t record=size_t(table)+size_t(entry)*64;
    const uint32_t rows=word(record+0x20),rowCount=bytes[record+0x30];
    if(!rows||!rowCount||uint64_t(rows)+uint64_t(rowCount)*32>bytes.size())return 0;
    size_t selected=rows;
    for(uint32_t index=0;index<rowCount;++index) {
        const size_t row=size_t(rows)+size_t(index)*32;
        uint64_t mask=0;std::memcpy(&mask,bytes.data()+row+8,8);
        if(mask&(uint64_t(1)<<weapon)) {selected=row;break;}
    }
    return bytes[selected+0x14];
}

