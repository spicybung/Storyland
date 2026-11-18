#pragma once
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

inline std::string storylandPickupVisualModelName(const std::string& modelName) {
    const auto slash=modelName.find_last_of("/\\");
    auto name=modelName.substr(slash==std::string::npos ? 0 : slash+1);
    const auto extension=name.find('.');
    if(extension!=std::string::npos)name.resize(extension);
    for(char& character:name)character=char(std::tolower(static_cast<unsigned char>(character)));
    return name;
}

template<class Weapons,class ResolveModel>
int storylandPickupWeaponNativeId(int32_t model,const std::string& name,
    const Weapons& weapons,ResolveModel resolveModel) {
    if(model<0)model=resolveModel(storylandPickupVisualModelName(name));
    if(model<0)return -1;
    for(const auto& weapon:weapons)if(weapon.modelId==model)return weapon.nativeId;
    return -1;
}

inline std::array<uint8_t,4> storylandPickupGlowColor(bool vcs,int weaponId,const std::string& modelName) {
    if(weaponId >= 0 && weaponId <= (vcs ? 39 : 36)) {
        std::array<uint8_t,3> rgb{};
        if(weaponId < 2) rgb={128,128,128};
        else if(weaponId < 12) rgb={97,194,247};
        else if(weaponId < (vcs ? 19 : 17)) rgb={27,89,130};
        else if(weaponId < (vcs ? 21 : 19)) rgb={149,194,24};
        else if(weaponId < (vcs ? 24 : 22)) rgb={45,155,90};
        else if(weaponId < (vcs ? 28 : 26)) rgb={255,227,79};
        else if(weaponId < (vcs ? 30 : 28)) rgb={254,137,0};
        else if(weaponId < (vcs ? 32 : 30)) rgb={249,131,215};
        else if(weaponId < (vcs ? 36 : 34)) rgb={164,40,178};
        else rgb={69,69,69};
        return {rgb[0],rgb[1],rgb[2],130};
    }
    const auto name=storylandPickupVisualModelName(modelName);
    if(name=="money")return {0,255,0,130};
    if(name=="bodyarmour")return {128,255,128,130};
    if(name=="health")return {255,80,150,130};
    return {255,220,120,110};
}

struct StorylandPickupCorona {
    std::array<float,3> position{};
    std::array<uint8_t,3> color{};
    float radius=0,range=65,pullAway=1.5f;
    uint8_t texture=0,alpha=255;
};

struct StorylandPickupGroundGlow {
    std::array<float,3> position{};
    std::array<uint8_t,3> color{};
    float frontX=2,frontY=0,sideX=0,sideY=-2;
    float downDistance=4,upDistance=0,range=40;
    bool visible=false;
};

struct StorylandVcsPickupVisual {
    std::array<float,16> matrix{};
    std::array<StorylandPickupCorona,6> coronas{};
    size_t coronaCount=0;
    StorylandPickupGroundGlow groundGlow;
    std::array<float,3> pointLightPosition{},pointLightColor{};
    float pointLightRadius=0,scale=1;
    bool valid=false;
};

inline float storylandPickupVisualFloat(uint32_t bits) {
    float result=0;
    std::memcpy(&result,&bits,sizeof(result));
    return result;
}

inline uint32_t storylandPickupVisualWord(const std::vector<uint8_t>& bytes,size_t offset) {
    uint32_t result=0;
    if(offset<=bytes.size()&&bytes.size()-offset>=4)
        std::memcpy(&result,bytes.data()+offset,sizeof(result));
    return result;
}

inline uint32_t storylandPickupVisualRandom(uint32_t& state) {
    const int32_t seed=int32_t(state);
    const int64_t next=int64_t(seed%127773)*16807-int64_t(seed/127773)*2836;
    state=uint32_t(next<=0?next+2147483647:next);
    return state;
}

inline bool storylandPickupVisualSpecialModel(const std::vector<uint8_t>& bytes,
    int32_t model,size_t offset) {
    const size_t specials=storylandPickupVisualWord(bytes,0x5C);
    if(!specials||specials>bytes.size()||offset+2>bytes.size()-specials)return false;
    return model==int32_t(uint16_t(bytes[specials+offset])|
        uint16_t(bytes[specials+offset+1])<<8);
}

inline bool storylandPickupVisualBounds(const std::vector<uint8_t>& bytes,int32_t model,
    std::array<float,3>& extent) {
    if(model<0||uint32_t(model)>=storylandPickupVisualWord(bytes,0x38))return false;
    const size_t table=storylandPickupVisualWord(bytes,0x3C);
    if(!table||table>bytes.size()||(uint64_t(model)+1)*4>bytes.size()-table)return false;
    const size_t info=storylandPickupVisualWord(bytes,table+size_t(model)*4);
    if(!info||info>bytes.size()||bytes.size()-info<0x18)return false;
    const size_t collision=storylandPickupVisualWord(bytes,info+0x14);
    if(!collision||collision>bytes.size()||bytes.size()-collision<0x2C)return false;
    for(size_t axis=0;axis<3;++axis) {
        const float minimum=storylandPickupVisualFloat(storylandPickupVisualWord(bytes,collision+0x10+axis*4));
        const float maximum=storylandPickupVisualFloat(storylandPickupVisualWord(bytes,collision+0x20+axis*4));
        extent[axis]=maximum-minimum;
        if(!std::isfinite(extent[axis])||extent[axis]<0)return false;
    }
    return std::max({extent[0],extent[1],extent[2]})>0;
}

inline std::array<uint8_t,3> storylandPickupVisualTint(int weaponId) {
    if(weaponId==40)return {255,100,100};
    if(weaponId==41)return {128,255,128};
    const auto color=storylandPickupGlowColor(true,std::clamp(weaponId,0,39),{});
    return {color[0],color[1],color[2]};
}

inline std::array<uint8_t,3> storylandPickupVisualMultiply(
    const std::array<uint8_t,3>& color,float factor) {
    return {uint8_t(float(color[0])*factor),uint8_t(float(color[1])*factor),
        uint8_t(float(color[2])*factor)};
}

// SLUS_215.90 pickup presentation: 1DF618, 1E0B38 and 1E0E08.
template<class Random>
StorylandVcsPickupVisual storylandVcsPickupVisual(const std::vector<uint8_t>& bytes,
    int32_t model,int weaponId,const std::array<float,3>& position,uint32_t milliseconds,
    uint32_t objectPhase,const std::array<float,3>& camera,Random random,bool suppressGlow=false) {
    StorylandVcsPickupVisual result;
    std::array<float,3> extent{};
    if(!storylandPickupVisualBounds(bytes,model,extent))return result;
    const bool package=storylandPickupVisualSpecialModel(bytes,model,0x4E);
    const bool money=storylandPickupVisualSpecialModel(bytes,model,0x50);
    bool weaponEffects=true;
    for(const size_t offset:{0x9Cu,0xACu,0xA6u,0xA0u,0xA8u,0xAAu,0xA2u,0xA4u,
        0xB2u,0xB4u,0xAEu,0xB0u,0xB6u,0x92u,0x94u,0x96u,0x84u,0x98u,0x9Au})
        if(storylandPickupVisualSpecialModel(bytes,model,offset))weaponEffects=false;
    if(storylandPickupVisualSpecialModel(bytes,model,0x9E))weaponId=41;
    const auto tint=storylandPickupVisualTint(weaponId);
    const float maximum=std::max({extent[0],extent[1],extent[2]});
    result.scale=package||money?1.0f:1.0f+(std::max(1.2f/maximum,1.0f)-1.0f)*.6f;
    if(model==0x16A||model==0x16E)result.scale=1.2f;
    const float angle=float(milliseconds&(package?4095u:2047u))*
        storylandPickupVisualFloat(package?0x3AC851ECu:0x3B4851ECu);
    const float sine=std::sin(angle)*result.scale,cosine=std::cos(angle)*result.scale;
    result.matrix={sine,cosine,0,0,-cosine,sine,0,0,0,0,result.scale,0,
        position[0],position[1],position[2],1};
    result.valid=true;
    result.groundGlow.position=position;
    result.pointLightPosition=position;
    if(package||money) {
        const float dx=camera[0]-position[0],dy=camera[1]-position[1],dz=camera[2]-position[2];
        const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
        const float range=package?14.0f:20.0f;
        if(distance>=range)return result;
        const uint32_t phase=(milliseconds+objectPhase)&(package?2047u:1023u);
        const float pulse=std::cos(float(phase)*storylandPickupVisualFloat(
            package?0x3B48F5C3u:0x3BC8F5C3u));
        const float brightness=(pulse*(package?.5f:.2f)+(package?.5f:.3f))*
            (range-distance)*(package?storylandPickupVisualFloat(0x3D924925u):.05f)*
            (package?255.0f:64.0f);
        const uint8_t intensity=uint8_t(std::clamp(brightness,0.0f,255.0f));
        const std::array<uint8_t,3> color=package?std::array<uint8_t,3>{intensity,intensity,intensity}:
            std::array<uint8_t,3>{0,intensity,0};
        result.coronas[0]={position,color,package?-.6f:-.4f,40,1.5f,uint8_t(package?5:7),255};
        result.coronaCount=1;
        result.groundGlow.color=color;
        result.groundGlow.visible=true;
        return result;
    }
    if(!weaponEffects)return result;
    const unsigned phase=(milliseconds>>9)&7u;
    const auto active=[&](unsigned value) {return value>3||(value==3&&(random()&3u)!=0);};
    const bool first=active(phase)&&!suppressGlow;
    const bool second=active((phase-1u)&7u)&&!suppressGlow;
    result.coronas[0]={position,first?storylandPickupVisualMultiply(tint,.45f):
        std::array<uint8_t,3>{},first?-.76f:-.57f,65,first?-.4f:1.5f,7,255};
    result.coronas[1]={position,second?storylandPickupVisualMultiply(tint,.55f):
        std::array<uint8_t,3>{},second?-.6f:-.45f,65,second?-.4f:1.5f,7,255};
    result.coronaCount=2;
    if(first||second) {
        result.groundGlow.visible=true;
        result.groundGlow.color=storylandPickupVisualMultiply(tint,first?.3f:.25f);
    }
    if(first) {
        result.pointLightRadius=3.0f+float(random()&15u)*.1f;
        for(size_t channel=0;channel<3;++channel)result.pointLightColor[channel]=float(tint[channel])/256.0f;
    }
    if(model==0x16E) {
        const float a=storylandPickupVisualFloat(0x3DA00707u);
        const float b=storylandPickupVisualFloat(0xBF04D6E2u);
        const float c=storylandPickupVisualFloat(0xBC9820C0u);
        const float sinA=std::sin(a),cosA=std::cos(a);
        const float sinB=std::sin(b),cosB=std::cos(b);
        const float sinC=std::sin(c),cosC=std::cos(c);
        const float cosCCosA=cosC*cosA,sinCCosA=sinC*cosA;
        const std::array<float,12> local{
            sinC*sinB-cosCCosA*cosB,cosC*sinB+sinCCosA*cosB,-sinA*cosB,0,
            sinC*cosB+cosCCosA*sinB,cosC*cosB-sinCCosA*sinB,sinA*sinB,0,
            cosC*sinA,-sinC*sinA,-cosA,0};
        const std::array<float,3> offset{storylandPickupVisualFloat(0x3F543958u),
            storylandPickupVisualFloat(0xBA83126Fu),storylandPickupVisualFloat(0x3E676C8Bu)};
        const auto base=result.matrix;
        for(size_t column=0;column<3;++column)for(size_t row=0;row<3;++row)
            result.matrix[column*4+row]=(base[row]*local[column*4]+
                base[4+row]*local[column*4+1])+base[8+row]*local[column*4+2];
        for(size_t row=0;row<3;++row)result.matrix[12+row]=((base[row]*offset[0]+
            base[4+row]*offset[1])+base[8+row]*offset[2])+position[row];
    }
    const size_t axis=extent[0]==maximum?0:extent[1]==maximum?1:2;
    for(unsigned index=0;index<4;++index) {
        std::array<float,3> point{result.matrix[12],result.matrix[13],result.matrix[14]};
        const float offset=float(index)*storylandPickupVisualFloat(0x3EAAAAABu)*extent[axis];
        for(size_t coordinate=0;coordinate<3;++coordinate)
            point[coordinate]+=result.matrix[axis*4+coordinate]*offset;
        result.coronas[result.coronaCount++]={point,storylandPickupVisualMultiply(tint,.15f),-1,65,-.5f,0,255};
    }
    return result;
}

template<class Environment,class Runtime>
std::vector<StorylandVcsPickupVisual> storylandBuildVcsPickupVisualFrame(
    const std::vector<uint8_t>& bytes,const Environment& environment,const Runtime& runtime,
    const std::array<float,3>& camera,uint32_t& randomState) {
    std::vector<StorylandVcsPickupVisual> result(runtime.pickups.size()+environment.pickups.size());
    const auto random=[&]() {return storylandPickupVisualRandom(randomState);};
    const auto inRenderRange=[&](const std::array<float,3>& position) {
        const float dx=position[0]-camera[0],dy=position[1]-camera[1];
        return dx*dx+dy*dy<=140.0f*140.0f;
    };
    if(runtime.pickupsEnabled)for(size_t index=0;index<runtime.pickups.size();++index) {
        const auto& pickup=runtime.pickups[index];
        if(pickup.respawn>0||pickup.ammunition<=0||pickup.weapon>=runtime.weapons.size())continue;
        const auto& weapon=runtime.weapons[pickup.weapon];
        const std::array<float,3> position{pickup.position.x,pickup.position.y,pickup.position.z+.5f};
        if(!inRenderRange(position))continue;
        result[index]=storylandVcsPickupVisual(bytes,weapon.modelId,weapon.nativeId,
            position,environment.gameMilliseconds,uint32_t(index),camera,random);
    }
    for(size_t index=0;index<environment.pickups.size();++index) {
        const auto& pickup=environment.pickups[index];
        if(pickup.unavailable||pickup.removed||pickup.type==0)continue;
        int32_t model=pickup.model;
        if(model<0&&environment.resolveObjectModel)model=environment.resolveObjectModel(model);
        if(model<0&&!pickup.modelNameOverride.empty()&&environment.modelIdByName)
            model=environment.modelIdByName(storylandPickupVisualModelName(pickup.modelNameOverride));
        const std::array<float,3> position{pickup.x,pickup.y,pickup.z};
        if(!inRenderRange(position))continue;
        int weaponId=0;
        for(const auto& weapon:runtime.weapons)if(weapon.modelId==model) {weaponId=weapon.nativeId;break;}
        result[runtime.pickups.size()+index]=storylandVcsPickupVisual(bytes,model,weaponId,
            position,environment.gameMilliseconds,uint32_t(pickup.handle),camera,random);
    }
    return result;
}
