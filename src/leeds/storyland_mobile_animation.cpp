#include "storyland_mobile_animation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace {
using Matrix = std::array<double, 16>;
Matrix identity() { Matrix m{}; m[0]=m[5]=m[10]=m[15]=1.0; return m; }
Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (size_t column=0; column<4; ++column)
        for (size_t row=0; row<4; ++row)
            for (size_t k=0; k<4; ++k) result[column*4+row] += a[k*4+row]*b[column*4+k];
    return result;
}
StorylandModelPoint transform(const Matrix& m, const StorylandModelPoint& p) {
    return {float(m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12]),
            float(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13]),
            float(m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14])};
}
Matrix inverse(const Matrix& m) {
    const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
    const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if (std::abs(determinant)<1e-12) throw std::runtime_error("Mobile bone has a singular bind matrix.");
    Matrix r=identity();
    r[0]=(e*i-f*h)/determinant; r[4]=(c*h-b*i)/determinant; r[8]=(b*f-c*e)/determinant;
    r[1]=(f*g-d*i)/determinant; r[5]=(a*i-c*g)/determinant; r[9]=(c*d-a*f)/determinant;
    r[2]=(d*h-e*g)/determinant; r[6]=(b*g-a*h)/determinant; r[10]=(a*e-b*d)/determinant;
    for(size_t row=0;row<3;++row) r[12+row]=-(r[row]*m[12]+r[4+row]*m[13]+r[8+row]*m[14]);
    return r;
}
float readFloat(const std::vector<uint8_t>& bytes,size_t offset) {
    if(offset>bytes.size() || bytes.size()-offset<4u) throw std::runtime_error("Mobile bone matrix is incomplete.");
    const uint32_t bits=uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8u)|(uint32_t(bytes[offset+2])<<16u)|(uint32_t(bytes[offset+3])<<24u);
    float value; std::memcpy(&value,&bits,4u);
    if(!std::isfinite(value)) throw std::runtime_error("Mobile bone matrix is not finite.");
    return value;
}
std::string normalizedName(std::string value) {
    std::string result;
    for(char c:value) { if(c>='A'&&c<='Z')c=char(c-'A'+'a'); if(c!=' '&&c!='_'&&c!='-'&&c!='~')result+=c; }
    if(result.compare(0,5,"bip01")==0)result.erase(0,5);
    if(result=="lfingers")result="lfinger"; if(result=="rfingers")result="rfinger";
    return result;
}
StorylandAnimKey sample(const StorylandAnimTrack& track,float seconds) {
    const auto& keys=track.keys;
    if(seconds<=keys.front().time)return keys.front();
    if(seconds>=keys.back().time)return keys.back();
    auto upper=std::upper_bound(keys.begin(),keys.end(),seconds,[](float time,const StorylandAnimKey& key){return time<key.time;});
    const auto& b=*upper; const auto& a=*(upper-1);
    const double factor=b.time>a.time ? (seconds-a.time)/(b.time-a.time) : 0.0;
    auto blend=[&](float x,float y){return float(x+(y-x)*factor);};
    StorylandAnimKey result;
    result.tx=blend(a.tx,b.tx); result.ty=blend(a.ty,b.ty); result.tz=blend(a.tz,b.tz);
    result.sx=blend(a.sx,b.sx); result.sy=blend(a.sy,b.sy); result.sz=blend(a.sz,b.sz);
    double qa[4]={a.qx,a.qy,a.qz,a.qw},qb[4]={b.qx,b.qy,b.qz,b.qw};
    double dot=0.0; for(size_t i=0;i<4;++i)dot+=qa[i]*qb[i];
    if(dot<0.0){for(double& value:qb)value=-value;dot=-dot;}
    double left=1.0-factor,right=factor;
    if(dot<0.9995){const double angle=std::acos(std::max(-1.0,std::min(1.0,dot))); const double sine=std::sin(angle);left=std::sin((1.0-factor)*angle)/sine;right=std::sin(factor*angle)/sine;}
    double q[4],length=0.0; for(size_t i=0;i<4;++i){q[i]=qa[i]*left+qb[i]*right;length+=q[i]*q[i];}
    length=std::sqrt(length); if(length<1e-12)throw std::runtime_error("Invalid mobile animation rotation.");
    result.qx=float(q[0]/length);result.qy=float(q[1]/length);result.qz=float(q[2]/length);result.qw=float(q[3]/length);
    return result;
}
Matrix poseLocal(const Matrix& bind,const StorylandAnimTrack& track,float seconds,bool lockTranslation) {
    Matrix result=bind;
    const auto key=sample(track,seconds);
    double scales[3];
    for(size_t axis=0;axis<3;++axis)scales[axis]=std::sqrt(bind[axis*4]*bind[axis*4]+bind[axis*4+1]*bind[axis*4+1]+bind[axis*4+2]*bind[axis*4+2]);
    if(track.channelFlags&4u){scales[0]=key.sx;scales[1]=key.sy;scales[2]=key.sz;}
    if(track.channelFlags&1u){
        const double x=key.qx,y=key.qy,z=key.qz,w=key.qw;
        result[0]=(1-2*(y*y+z*z))*scales[0]; result[1]=2*(x*y+z*w)*scales[0]; result[2]=2*(x*z-y*w)*scales[0];
        result[4]=2*(x*y-z*w)*scales[1]; result[5]=(1-2*(x*x+z*z))*scales[1]; result[6]=2*(y*z+x*w)*scales[1];
        result[8]=2*(x*z+y*w)*scales[2]; result[9]=2*(y*z-x*w)*scales[2]; result[10]=(1-2*(x*x+y*y))*scales[2];
    } else if(track.channelFlags&4u) {
        for(size_t axis=0;axis<3;++axis){const double length=std::sqrt(bind[axis*4]*bind[axis*4]+bind[axis*4+1]*bind[axis*4+1]+bind[axis*4+2]*bind[axis*4+2]);if(length<1e-12)throw std::runtime_error("Invalid mobile bind scale.");for(size_t row=0;row<3;++row)result[axis*4+row]=bind[axis*4+row]*scales[axis]/length;}
    }
    if((track.channelFlags&2u)&&!lockTranslation){result[12]=key.tx;result[13]=key.ty;result[14]=key.tz;}
    return result;
}
}

bool storylandSampleMobileAnimation(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage) {
    try {
        if(!model.isMobileLcsDff() || !std::isfinite(seconds))throw std::runtime_error("Expected a mobile DFF and a finite animation time.");
        const auto& bones=model.armatureBones(); const auto& bytes=model.rawBytes();
        std::vector<Matrix> local(bones.size()),bindWorld(bones.size()),poseWorld(bones.size()),deltas(bones.size());
        std::vector<uint8_t> state(bones.size(),0u);
        for(size_t index=0;index<bones.size();++index){
            local[index]=identity();
            for(size_t vector=0;vector<4;++vector)for(size_t axis=0;axis<3;++axis)local[index][vector*4+axis]=readFloat(bytes,size_t(bones[index].offset)+vector*12u+axis*4u);
        }
        std::function<void(size_t)> compose=[&](size_t index){
            if(state[index]==2u)return; if(state[index]==1u)throw std::runtime_error("Mobile frame hierarchy contains a cycle."); state[index]=1u;
            const auto& bone=bones[index]; const StorylandAnimTrack* matched=nullptr;
            if(bone.boneId!=0xFFFFFFFFu)for(const auto& track:animation.tracks())if(track.boneId==bone.boneId&&!track.keys.empty()){matched=&track;break;}
            if(!matched)for(const auto& track:animation.tracks())if(!track.keys.empty()&&normalizedName(track.name)==normalizedName(bone.name)){matched=&track;break;}
            Matrix posed=matched?poseLocal(local[index],*matched,seconds,lockRootMotion&&bone.boneId==0u):local[index];
            if(bone.parentIndex!=0xFFFFFFFFu){if(bone.parentIndex>=bones.size())throw std::runtime_error("Mobile frame parent is out of range.");compose(bone.parentIndex);bindWorld[index]=multiply(bindWorld[bone.parentIndex],local[index]);poseWorld[index]=multiply(poseWorld[bone.parentIndex],posed);}else{bindWorld[index]=local[index];poseWorld[index]=posed;}
            deltas[index]=multiply(poseWorld[index],inverse(bindWorld[index])); state[index]=2u;
        };
        for(size_t index=0;index<bones.size();++index)compose(index);
        animatedBones.resize(bones.size()); for(size_t index=0;index<bones.size();++index)animatedBones[index]={float(poseWorld[index][12]),float(poseWorld[index][13]),float(poseWorld[index][14])};
        const auto& skin=model.previewSkinWeights(); const auto& owners=model.previewFrameIndices();
        animatedPoints=sourcePoints;
        for(size_t index=0;index<sourcePoints.size();++index){
            StorylandModelPoint result{}; double total=0.0;
            if(index<skin.size()&&skin[index].valid)for(uint32_t influence=0;influence<std::min(4u,skin[index].influenceCount);++influence){const auto& item=skin[index].influences[influence];if(item.weight<=0.0f||item.boneIndex>=deltas.size())continue;const auto p=transform(deltas[item.boneIndex],sourcePoints[index]);result.x+=p.x*item.weight;result.y+=p.y*item.weight;result.z+=p.z*item.weight;total+=item.weight;}
            if(total>0.0)animatedPoints[index]={float(result.x/total),float(result.y/total),float(result.z/total)};
            else if(index<owners.size()&&owners[index]<deltas.size())animatedPoints[index]=transform(deltas[owners[index]],sourcePoints[index]);
            const auto& p=animatedPoints[index];if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Mobile animation produced a non-finite position.");
        }
        errorMessage.clear();return true;
    } catch(const std::exception& exception){errorMessage=exception.what();animatedPoints=sourcePoints;animatedBones.clear();return false;}
}

namespace {
uint32_t nativeU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size()-offset < 4u) throw std::runtime_error("Model pointer is outside the file.");
    return uint32_t(bytes[offset]) | (uint32_t(bytes[offset+1])<<8u) |
        (uint32_t(bytes[offset+2])<<16u) | (uint32_t(bytes[offset+3])<<24u);
}
Matrix nativeMatrix(const std::vector<uint8_t>& bytes, size_t offset) {
    Matrix result=identity();
    for(size_t column=0; column<4; ++column)
        for(size_t row=0; row<3; ++row) result[column*4+row]=readFloat(bytes,offset+column*16u+row*4u);
    return result;
}
const StorylandAnimTrack* nativeTrack(const StorylandModelBone& bone,const StorylandAnimFile& animation) {
    // Names resolve retail jaw/toe aliases without ever falling back to track order.
    for(const auto& track:animation.tracks())
        if(!track.keys.empty() && normalizedName(track.name)==normalizedName(bone.name))return &track;
    for(const auto& track:animation.tracks())
        if(!track.keys.empty() && bone.boneId!=0xFFFFFFFFu && track.boneId==bone.boneId)return &track;
    return nullptr;
}
}

bool storylandSampleModelAnimation(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage) {
    if(model.isMobileLcsDff())return storylandSampleMobileAnimation(model,animation,seconds,lockRootMotion,sourcePoints,animatedPoints,animatedBones,errorMessage);
    try {
        const auto& bones=model.armatureBones();const auto& bytes=model.rawBytes();
        if(bytes.size()<4u || nativeU32(bytes,0)!=0x006D646Cu || bones.empty() || !std::isfinite(seconds))
            throw std::runtime_error("Open a Leeds model with a skeleton to preview this animation.");
        Matrix atomic=identity();size_t paletteOffset=0u;uint32_t paletteCount=0u;
        for(size_t offset=0;offset+0x34u<=bytes.size();offset+=4u){
            const uint32_t tag=nativeU32(bytes,offset);
            if(tag!=0x0004AA01u && tag!=0x0000AA01u && tag!=0x01050001u && (tag&0xFFFF00FFu)!=0x00040001u)continue;
            const uint32_t frame=nativeU32(bytes,offset+4u),geometry=nativeU32(bytes,offset+0x14u),hierarchy=nativeU32(bytes,offset+0x2Cu);
            if(!frame || uint64_t(frame)+0x90u>bytes.size() || !geometry || uint64_t(geometry)+0x20u>bytes.size() || !hierarchy || uint64_t(hierarchy)+0x38u>bytes.size())continue;
            if(nativeU32(bytes,hierarchy)!=0x3000u)continue;
            const uint32_t anchor=nativeU32(bytes,hierarchy+0x34u);
            if(std::none_of(bones.begin(),bones.end(),[&](const StorylandModelBone& bone){return bone.offset==anchor;}))continue;
            atomic=nativeMatrix(bytes,size_t(frame)+0x50u);
            const uint32_t auxiliary=nativeU32(bytes,size_t(geometry)+0x18u);
            if(auxiliary && uint64_t(auxiliary)+16u<=bytes.size() && nativeU32(bytes,auxiliary)==0x19u){
                paletteOffset=nativeU32(bytes,size_t(auxiliary)+12u);paletteCount=nativeU32(bytes,size_t(hierarchy)+4u);
                if(!paletteOffset || paletteCount>256u || uint64_t(paletteOffset)+uint64_t(paletteCount)*64u>bytes.size())
                    throw std::runtime_error("The model's bone matrix table is incomplete.");
            }
            break;
        }
        const Matrix inverseAtomic=inverse(atomic);
        std::vector<Matrix> bindLocal(bones.size()),bindWorld(bones.size()),poseWorld(bones.size());
        std::vector<uint8_t> state(bones.size(),0u);
        std::function<void(size_t)> compose=[&](size_t index){
            if(state[index]==2u)return;
            if(state[index]==1u)throw std::runtime_error("Model skeleton contains a parent cycle.");
            state[index]=1u;const auto& bone=bones[index];
            bindLocal[index]=nativeMatrix(bytes,size_t(bone.offset)+0x10u);
            const auto* track=nativeTrack(bone,animation);
            Matrix local=track?poseLocal(bindLocal[index],*track,seconds,lockRootMotion&&bone.boneId==0u):bindLocal[index];
            if(bone.parentIndex!=0xFFFFFFFFu){
                if(bone.parentIndex>=bones.size())throw std::runtime_error("Model bone parent is out of range.");
                compose(bone.parentIndex);bindWorld[index]=multiply(bindWorld[bone.parentIndex],bindLocal[index]);poseWorld[index]=multiply(poseWorld[bone.parentIndex],local);
            }else{bindWorld[index]=bindLocal[index];poseWorld[index]=local;}
            state[index]=2u;
        };
        for(size_t index=0;index<bones.size();++index)compose(index);
        animatedBones.resize(bones.size());
        for(size_t index=0;index<bones.size();++index)animatedBones[index]=transform(inverseAtomic,{float(poseWorld[index][12]),float(poseWorld[index][13]),float(poseWorld[index][14])});
        // PS2 skin tokens use the documented HAnim palette, not file allocation
        // order. PSP tokens have already been resolved by the model reader.
        const bool jaw=std::any_of(bones.begin(),bones.end(),[](const StorylandModelBone& b){return normalizedName(b.name)=="jaw";});
        static const uint32_t withJaw[]={0,1,2,3,4,5,8,31,32,33,34,35,21,22,23,24,25,41,42,43,54,51,52,53,55};
        static const uint32_t withoutJaw[]={0,1,2,3,4,5,31,32,33,34,35,21,22,23,24,25,41,42,43,54,51,52,53,55};
        std::vector<Matrix> skinMatrices;
        std::vector<uint32_t> paletteBones;
        const size_t count=model.skinWeightsUseBoneIndices()?bones.size():(jaw?25u:24u);
        for(size_t index=0;index<count;++index){
            uint32_t boneIndex=0xFFFFFFFFu;
            if(model.skinWeightsUseBoneIndices())boneIndex=uint32_t(index);
            else{const uint32_t id=jaw?withJaw[index]:withoutJaw[index];for(uint32_t b=0;b<bones.size();++b)if(bones[b].boneId==id){boneIndex=b;break;}}
            paletteBones.push_back(boneIndex);skinMatrices.push_back(identity());
            if(boneIndex==0xFFFFFFFFu)continue;
            const size_t matrixIndex=model.skinWeightsUseBoneIndices()?bones[boneIndex].nodeId:index;
            const Matrix bind=(paletteOffset && matrixIndex<paletteCount)?nativeMatrix(bytes,paletteOffset+matrixIndex*64u):multiply(inverse(bindWorld[boneIndex]),atomic);
            skinMatrices.back()=multiply(inverseAtomic,multiply(poseWorld[boneIndex],bind));
        }
        animatedPoints=sourcePoints;const auto& skin=model.previewSkinWeights();
        for(size_t index=0;index<sourcePoints.size() && index<skin.size();++index){
            const auto& weights=skin[index];if(!weights.valid)continue;
            StorylandModelPoint result{};double total=0.0;
            for(uint32_t influence=0;influence<std::min(4u,weights.influenceCount);++influence){
                const auto& item=weights.influences[influence];
                if(item.weight<=0.0f)continue;
                if(item.boneIndex>=skinMatrices.size() || paletteBones[item.boneIndex]==0xFFFFFFFFu)
                    throw std::runtime_error("A vertex refers to a bone missing from this model.");
                const auto p=transform(skinMatrices[item.boneIndex],sourcePoints[index]);
                result.x+=p.x*item.weight;result.y+=p.y*item.weight;result.z+=p.z*item.weight;total+=item.weight;
            }
            if(total>0.0)animatedPoints[index]={float(result.x/total),float(result.y/total),float(result.z/total)};
            const auto& p=animatedPoints[index];if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Animation produced an invalid vertex position.");
        }
        errorMessage.clear();return true;
    }catch(const std::exception& exception){errorMessage=exception.what();animatedPoints=sourcePoints;animatedBones.clear();return false;}
}
