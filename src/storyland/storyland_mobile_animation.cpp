#include "src/storyland/storyland_mobile_animation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
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
    StorylandModelPoint result{float(m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12]),
        float(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13]),
        float(m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14])};
    storylandTransformPs2ModelNormal(result,p,m.data(),false);
    if(p.hasNormal) {
        const double determinant=m[0]*(m[5]*m[10]-m[9]*m[6])-
            m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);
        if(std::isfinite(determinant)&&std::abs(determinant)>1e-12) {
            result.nx=float(((m[5]*m[10]-m[9]*m[6])*p.nx+
                (m[9]*m[2]-m[1]*m[10])*p.ny+(m[1]*m[6]-m[5]*m[2])*p.nz)/determinant);
            result.ny=float(((m[8]*m[6]-m[4]*m[10])*p.nx+
                (m[0]*m[10]-m[8]*m[2])*p.ny+(m[4]*m[2]-m[0]*m[6])*p.nz)/determinant);
            result.nz=float(((m[4]*m[9]-m[8]*m[5])*p.nx+
                (m[8]*m[1]-m[0]*m[9])*p.ny+(m[0]*m[5]-m[4]*m[1])*p.nz)/determinant);
            result.hasNormal=true;
        }
    }
    return result;
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
std::string normalizedName(const std::string& value) {
    std::string result;
    for(char c:value) { if(c>='A'&&c<='Z')c=char(c-'A'+'a'); if(c!=' '&&c!='_'&&c!='-'&&c!='~')result+=c; }
    if(result.compare(0,5,"bip01")==0)result.erase(0,5);
    if(result=="lfingers")result="lfinger";
    if(result=="rfingers")result="rfinger";
    return result;
}
bool nativeWeaponUpperBodyBone(const std::vector<StorylandModelBone>& bones,size_t index) {
    for(size_t depth=0;index<bones.size()&&depth<bones.size();++depth) {
        const auto& bone=bones[index];
        if(bone.boneId==3u || (bone.boneId==UINT32_MAX&&normalizedName(bone.name)=="spine1"))return true;
        index=bone.parentIndex;
    }
    return false;
}
float upperBodyBoneWeight(const std::string& name) {
    if(name.find("leg")!=std::string::npos || name.find("thigh")!=std::string::npos ||
       name.find("foot")!=std::string::npos || name.find("toe")!=std::string::npos ||
       name.find("pelvis")!=std::string::npos || name.find("root")!=std::string::npos ||
       name=="hips") return 0.0f;
    if(name=="spine" || name=="spine0") return 0.2f;
    if(name=="spine1") return 0.45f;
    if(name=="spine2" || name=="spine3" || name=="chest") return 0.8f;
    if(name.find("arm")!=std::string::npos || name.find("hand")!=std::string::npos ||
       name.find("clavicle")!=std::string::npos || name.find("finger")!=std::string::npos) return 1.0f;
    if(name.find("neck")!=std::string::npos || name.find("head")!=std::string::npos ||
       name.find("jaw")!=std::string::npos) return 0.55f;
    return 0.0f;
}
float upperBodyBoneWeight(const StorylandModelBone& bone) {
    return upperBodyBoneWeight(normalizedName(bone.name));
}

// These names exist only for the current pose. Resolving a skeleton used to
// normalize every track again for every bone and every animation layer.
struct AnimationTracks {
    const std::vector<StorylandAnimTrack>* tracks=nullptr;
    std::vector<std::string> names;
    explicit AnimationTracks(const StorylandAnimFile* value,
        const std::vector<StorylandAnimTrack>* selected=nullptr) : tracks(selected?selected:value?&value->tracks():nullptr) {
        if(!tracks)return;
        names.reserve(tracks->size());
        for(const auto& track:*tracks)names.push_back(normalizedName(track.name));
    }
    const StorylandAnimTrack* find(const StorylandModelBone& bone,const std::string& name) const {
        if(!tracks)return nullptr;
        if(bone.boneId!=0xFFFFFFFFu)for(const auto& track:*tracks)
            if(track.hasNativeBoneTag&&!track.keys.empty()&&track.boneId==bone.boneId)return &track;
        for(size_t index=0;index<tracks->size();++index)
            if(!(*tracks)[index].hasNativeBoneTag&&!(*tracks)[index].keys.empty()&&
                names[index]==name)return &(*tracks)[index];
        if(bone.boneId!=0xFFFFFFFFu)for(const auto& track:*tracks)
            if(!track.keys.empty()&&track.boneId==bone.boneId)return &track;
        return nullptr;
    }
};

bool activePoseTrack(const StorylandAnimTrack* track, float weight, bool lockTranslation,
    bool includeScale = true) {
    return track && !track->keys.empty() && weight > 0 &&
        (((track->channelFlags & 7u) == 0) || (track->channelFlags & 1u) ||
            (includeScale && (track->channelFlags & 4u)) ||
            (!lockTranslation && (track->channelFlags & 2u)));
}

StorylandAnimKey sample(const StorylandAnimTrack& track,float seconds) {
    const auto& keys=track.keys;
    if(seconds<=keys.front().time)return keys.front();
    const bool fileOrderKeys = track.source == "ANPK IFP (file-order keys)";
    if (!fileOrderKeys && seconds>=keys.back().time)return keys.back();
    const auto upper = storylandNextAnimKey(track, seconds);
    if (upper == keys.end()) return keys.back();
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
float nativeSequenceSampleTime(const StorylandAnimTrack& track,float seconds) {
    // Both compressed FindKeyFrame functions begin with the interval at key1.
    return seconds+((track.keyStride==10u||track.keyStride==16u)&&!track.keys.empty()?
        track.keys.front().time:0.0f);
}
float nativeCompressedTranslationDelta(float first,float last) {
    const auto decode=[](float value) {
        const float magnitude=std::abs(value);
        return magnitude>0&&magnitude<0x1p-14f?
            std::copysign(0x1p-15f+magnitude*.5f,value):value;
    };
    const float delta=decode(last)-decode(first);
    uint32_t bits=0;
    std::memcpy(&bits,&delta,sizeof(bits));
    const int exponent=int((bits>>23)&255u)-127;
    // LCS145AC0/VCS478D78 truncate their intermediate half and flush underflow.
    bits=exponent < -14 ? bits&0x80000000u : bits&0xFFFFE000u;
    float result=0;
    std::memcpy(&result,&bits,sizeof(result));
    return result;
}
StorylandAnimKey sampleNativeSequenceKey(const StorylandAnimTrack& track,float seconds) {
    const float time=nativeSequenceSampleTime(track,seconds);
    auto result=sample(track,time);
    if(track.keyStride!=16u||!(track.channelFlags&2u)||track.keys.size()<2)return result;
    const auto& keys=track.keys;
    if(time<=keys.front().time)return result;
    auto upper=std::lower_bound(keys.begin()+1,keys.end(),time,
        [](const auto& key,float value){return key.time<value;});
    if(upper==keys.end())upper=keys.end()-1;
    const auto& first=*(upper-1);
    const auto& last=*upper;
    const float factor=last.time>first.time?
        std::clamp((time-first.time)/(last.time-first.time),0.0f,1.0f):0.0f;
    result.tx=first.tx+nativeCompressedTranslationDelta(first.tx,last.tx)*factor;
    result.ty=first.ty+nativeCompressedTranslationDelta(first.ty,last.ty)*factor;
    result.tz=first.tz+nativeCompressedTranslationDelta(first.tz,last.tz)*factor;
    return result;
}
Matrix poseLocalKey(const Matrix& bind,const StorylandAnimKey& key,uint32_t channelFlags,bool lockTranslation) {
    Matrix result=bind;
    double scales[3];
    for(size_t axis=0;axis<3;++axis)scales[axis]=std::sqrt(bind[axis*4]*bind[axis*4]+bind[axis*4+1]*bind[axis*4+1]+bind[axis*4+2]*bind[axis*4+2]);
    if(channelFlags&4u){scales[0]=key.sx;scales[1]=key.sy;scales[2]=key.sz;}
    if(channelFlags&1u){
        const double x=key.qx,y=key.qy,z=key.qz,w=key.qw;
        result[0]=(1-2*(y*y+z*z))*scales[0]; result[1]=2*(x*y+z*w)*scales[0]; result[2]=2*(x*z-y*w)*scales[0];
        result[4]=2*(x*y-z*w)*scales[1]; result[5]=(1-2*(x*x+z*z))*scales[1]; result[6]=2*(y*z+x*w)*scales[1];
        result[8]=2*(x*z+y*w)*scales[2]; result[9]=2*(y*z-x*w)*scales[2]; result[10]=(1-2*(x*x+y*y))*scales[2];
    } else if(channelFlags&4u) {
        for(size_t axis=0;axis<3;++axis){const double length=std::sqrt(bind[axis*4]*bind[axis*4]+bind[axis*4+1]*bind[axis*4+1]+bind[axis*4+2]*bind[axis*4+2]);if(length<1e-12)throw std::runtime_error("Invalid mobile bind scale.");for(size_t row=0;row<3;++row)result[axis*4+row]=bind[axis*4+row]*scales[axis]/length;}
    }
    if((channelFlags&2u)&&!lockTranslation){result[12]=key.tx;result[13]=key.ty;result[14]=key.tz;}
    return result;
}
Matrix poseLocal(const Matrix& bind,const StorylandAnimTrack& track,float seconds,bool lockTranslation) {
    return poseLocalKey(bind,sample(track,seconds),track.channelFlags,lockTranslation);
}

Matrix blendedPoseLocal(const Matrix& bind, const StorylandAnimTrack* first, float firstSeconds,
    const StorylandAnimTrack* second, float secondSeconds, float weight, bool lockTranslation,
    const StorylandAnimTrack* upper = nullptr, float upperSeconds = 0, float upperWeight = 0) {
    if (!first && !second) return bind;
    if (!first) { first=second; firstSeconds=secondSeconds; }
    if (!second) { second=first; secondSeconds=firstSeconds; }
    weight=std::clamp(weight,0.0f,1.0f);
    const StorylandAnimKey a = sample(*first, firstSeconds);
    const StorylandAnimKey b = sample(*second, secondSeconds);
    StorylandAnimKey key;
    const auto lerp = [weight](float x, float y) { return x + (y - x) * weight; };
    key.tx = lerp(a.tx, b.tx); key.ty = lerp(a.ty, b.ty); key.tz = lerp(a.tz, b.tz);
    key.sx = lerp(a.sx, b.sx); key.sy = lerp(a.sy, b.sy); key.sz = lerp(a.sz, b.sz);
    // Use the shortest-arc quaternion interpolation, as for RenderWare HAnim.
    double qa[4] = {a.qx, a.qy, a.qz, a.qw};
    double qb[4] = {b.qx, b.qy, b.qz, b.qw};
    double dot = 0;
    for (size_t axis = 0; axis < 4; ++axis) dot += qa[axis] * qb[axis];
    if (dot < 0) { for (double& component : qb) component = -component; dot = -dot; }
    const double angle = dot < 0.9995 ? std::acos(std::clamp(dot, -1.0, 1.0)) : 0;
    const double sine = angle > 0 ? std::sin(angle) : 1;
    const double left = angle > 0 ? std::sin((1.0 - weight) * angle) / sine : 1.0 - weight;
    const double right = angle > 0 ? std::sin(weight * angle) / sine : weight;
    double quaternion[4], length = 0;
    for (size_t axis = 0; axis < 4; ++axis) {
        quaternion[axis] = qa[axis] * left + qb[axis] * right;
        length += quaternion[axis] * quaternion[axis];
    }
    if (length < 1e-12) throw std::runtime_error("Blended animation rotation is degenerate.");
    length = std::sqrt(length);
    key.qx = float(quaternion[0] / length); key.qy = float(quaternion[1] / length);
    key.qz = float(quaternion[2] / length); key.qw = float(quaternion[3] / length);
    if (upper && (upper->channelFlags & 1u) && upperWeight > 0.0f) {
        const StorylandAnimKey upperKey=sample(*upper, upperSeconds);
        double destination[4]={upperKey.qx,upperKey.qy,upperKey.qz,upperKey.qw};
        double source[4]={key.qx,key.qy,key.qz,key.qw};
        double rotationDot=0;
        for(size_t axis=0;axis<4;++axis)rotationDot+=source[axis]*destination[axis];
        if(rotationDot<0){for(double& component:destination)component=-component;rotationDot=-rotationDot;}
        const double rotationAngle=rotationDot<0.9995?std::acos(std::clamp(rotationDot,-1.0,1.0)):0;
        const double rotationSine=rotationAngle>0?std::sin(rotationAngle):1;
        const double factor=std::clamp(double(upperWeight),0.0,1.0);
        const double sourceWeight=rotationAngle>0?std::sin((1-factor)*rotationAngle)/rotationSine:1-factor;
        const double targetWeight=rotationAngle>0?std::sin(factor*rotationAngle)/rotationSine:factor;
        double blended[4],magnitude=0;
        for(size_t axis=0;axis<4;++axis){blended[axis]=source[axis]*sourceWeight+destination[axis]*targetWeight;magnitude+=blended[axis]*blended[axis];}
        if(magnitude<1e-12)throw std::runtime_error("Layered animation rotation is degenerate.");
        magnitude=std::sqrt(magnitude);
        key.qx=float(blended[0]/magnitude);key.qy=float(blended[1]/magnitude);
        key.qz=float(blended[2]/magnitude);key.qw=float(blended[3]/magnitude);
    }
    uint32_t channelFlags=first->channelFlags|second->channelFlags;
    if(upper&&upperWeight>0.0f)channelFlags|=upper->channelFlags&1u;
    return poseLocalKey(bind,key,channelFlags,lockTranslation);
}
Matrix axisRotation(double x,double y,double z,double degrees) {
    const double angle=degrees*0.0174532925199433,c=std::cos(angle),s=std::sin(angle),t=1-c;
    Matrix result=identity();
    result[0]=t*x*x+c;result[1]=t*x*y+s*z;result[2]=t*x*z-s*y;
    result[4]=t*x*y-s*z;result[5]=t*y*y+c;result[6]=t*y*z+s*x;
    result[8]=t*x*z+s*y;result[9]=t*y*z-s*x;result[10]=t*z*z+c;
    return result;
}
std::array<double,4> rotationQuaternion(const Matrix& matrix) {
    Matrix m=matrix;
    for(size_t axis=0;axis<3;++axis) {
        const double length=std::sqrt(m[axis*4]*m[axis*4]+m[axis*4+1]*m[axis*4+1]+m[axis*4+2]*m[axis*4+2]);
        if(length<1e-12)throw std::runtime_error("Aim bone has a singular rotation.");
        for(size_t row=0;row<3;++row)m[axis*4+row]/=length;
    }
    std::array<double,4> q{};
    const double trace=m[0]+m[5]+m[10];
    if(trace>0) {
        const double scale=std::sqrt(trace+1)*2;
        q={(m[6]-m[9])/scale,(m[8]-m[2])/scale,(m[1]-m[4])/scale,scale/4};
    } else {
        const size_t i=m[0]>m[5]?(m[0]>m[10]?0:2):(m[5]>m[10]?1:2);
        const size_t j=(i+1)%3,k=(i+2)%3;
        const double scale=std::sqrt(1+m[i*4+i]-m[j*4+j]-m[k*4+k])*2;
        q[i]=scale/4;q[j]=(m[j*4+i]+m[i*4+j])/scale;
        q[k]=(m[k*4+i]+m[i*4+k])/scale;q[3]=(m[j*4+k]-m[k*4+j])/scale;
    }
    return q;
}
Matrix blendNativeRotation(const Matrix& original,const Matrix& destination,float weight) {
    auto first=rotationQuaternion(original),second=rotationQuaternion(destination);
    double dot=0;for(size_t i=0;i<4;++i)dot+=first[i]*second[i];
    if(dot<0) {dot=-dot;for(auto& value:second)value=-value;}
    const double angle=dot<.9995?std::acos(std::clamp(dot,-1.0,1.0)):0;
    const double a=angle>0?std::sin((1-weight)*angle)/std::sin(angle):1-weight;
    const double b=angle>0?std::sin(weight*angle)/std::sin(angle):weight;
    double length=0;for(size_t i=0;i<4;++i) {first[i]=first[i]*a+second[i]*b;length+=first[i]*first[i];}
    length=std::sqrt(length);
    if(length<1e-12)throw std::runtime_error("Aim bone has an invalid blended rotation.");
    StorylandAnimKey key;key.qx=float(first[0]/length);key.qy=float(first[1]/length);
    key.qz=float(first[2]/length);key.qw=float(first[3]/length);
    return poseLocalKey(original,key,1,true);
}
void nativeWeaponAim(const std::vector<StorylandModelBone>& bones,std::vector<Matrix>& world,
    const Matrix& modelFrame,const StorylandPedAimPose* aim) {
    if(!aim||!aim->state||aim->weight<=0)return;
    auto& state=*aim->state;
    const float values[]={state.torsoYaw,state.torsoPitch,state.upperYaw,state.upperPitch,
        state.lowerYaw,state.lowerPitch,state.stepFraction,state.upperRoll,state.handRoll};
    if(!std::all_of(std::begin(values),std::end(values),[](float value){return std::isfinite(value);}))
        throw std::runtime_error("Native aiming has an invalid joint state.");
    const float weight=std::clamp(aim->weight,0.0f,1.0f);
    const auto indexOf=[&](uint32_t id) {
        for(size_t index=0;index<bones.size();++index)if(bones[index].boneId==id)return index;
        return SIZE_MAX;
    };
    const size_t torso=indexOf(3),clavicle=indexOf(21),upper=indexOf(22),lower=indexOf(23),hand=indexOf(24),spine=indexOf(2);
    if(torso==SIZE_MAX||spine==SIZE_MAX||(aim->arm&&
        (clavicle==SIZE_MAX||upper==SIZE_MAX||lower==SIZE_MAX||hand==SIZE_MAX)))
        throw std::runtime_error("Native weapon aiming requires the authored torso and arm bones.");
    const auto wrap=[](float degrees) {return std::remainder(degrees,360.0f);};
    // CPedIK::MoveLimb, SLUS_215.90 0x2EDF98. The PS2 limits and per-frame
    // steps are also present unchanged in SLUS_214.23. Run their coupled
    // limb adjustments at the PS2's 30Hz, independently of repaint frequency.
    state.stepFraction+=std::clamp(aim->seconds,0.0f,.1f)*30;
    const unsigned ticks=unsigned(state.stepFraction+1e-5f);
    state.stepFraction=std::max(0.0f,state.stepFraction-float(ticks));
    const auto limb=[&](float& yaw,float& pitch,float desiredYaw,float desiredPitch,
        float minimumYaw,float maximumYaw,float yawStep,float minimumPitch,float maximumPitch,float pitchStep) {
        // Preserve MoveLimb's return state as well as its angles: the arm
        // solver starts the forearm correction only after a clamped result.
        int result=1;
        if(std::fabs(yaw-desiredYaw)<yawStep) {yaw=desiredYaw;result=2;}
        else if(yaw<desiredYaw)yaw+=yawStep;
        else if(yaw>desiredYaw)yaw-=yawStep;
        if(yaw<minimumYaw||yaw>maximumYaw) {
            yaw=std::clamp(yaw,minimumYaw,maximumYaw);result=0;
        }
        if(std::fabs(pitch-desiredPitch)<pitchStep)pitch=desiredPitch;
        else {
            if(pitch<desiredPitch)pitch+=pitchStep;
            else if(pitch>desiredPitch)pitch-=pitchStep;
            result=1;
        }
        if(pitch<minimumPitch||pitch>maximumPitch) {
            pitch=std::clamp(pitch,minimumPitch,maximumPitch);result=0;
        }
        return result;
    };
    const Matrix facing=multiply(axisRotation(0,0,1,90-aim->modelForwardAngle),modelFrame);
    const auto facingFrame=[&](size_t index) {return multiply(facing,world[index]);};
    if(aim->aiming)state.initialized=true;
    if(!state.initialized)return;
    for(unsigned tick=0;tick<ticks;++tick) {
    float yaw=wrap(aim->yaw),pitch=-aim->pitch;
    bool armReached=false;
    if(aim->arm&&aim->aiming) {
        state.lowerActive=false;state.upperRoll=45;state.handRoll=30;
        const auto parent=facingFrame(clavicle);
        const float parentYaw=float(std::atan2(parent[1],parent[0])*57.2957795131);
        const float parentPitch=float(std::asin(std::clamp(parent[6],-1.0,1.0))*57.2957795131);
        const float armYaw=wrap(yaw-parentYaw),armPitch=wrap(pitch-parentPitch);
        const int result=limb(state.upperYaw,state.upperPitch,armYaw,armPitch,-120,5,20,-70,70,20);
        armReached=result==2;
        if(result==0) {
            state.lowerActive=true;
            const float remainder=armYaw-state.upperYaw;
            int lowerResult;
            if(remainder>0) {
                const float factor=1-std::min(remainder/60.0f,1.0f);
                state.upperRoll*=factor;state.handRoll*=factor;
                lowerResult=limb(state.lowerYaw,state.lowerPitch,remainder*1.9f,0,0,60,15,-90,90,10);
                const float scale=std::max(0.0f,1-.34f*(state.lowerYaw+state.upperYaw)*.0174532925199433f);
                state.upperPitch=std::min(state.upperPitch+.2f*state.lowerYaw,70*scale);
            } else lowerResult=limb(state.lowerYaw,state.lowerPitch,remainder,0,0,60,15,-90,90,10);
            armReached=armReached||lowerResult==2;
        }
        yaw=wrap(yaw-state.upperYaw-state.lowerYaw);
        if(!armReached)pitch=pitch>0?std::max(0.0f,pitch-std::fabs(yaw)):
            std::min(0.0f,pitch+std::fabs(yaw));
    }
    if(!aim->aiming)limb(state.torsoYaw,state.torsoPitch,0,0,-50,50,8,-45,45,5);
    else if(!aim->arm||!armReached)limb(state.torsoYaw,state.torsoPitch,yaw,pitch,-50,50,8,-45,45,5);
    else if(state.torsoYaw*state.upperYaw<0)
        limb(state.torsoYaw,state.torsoPitch,0,state.torsoPitch,-50,50,8,-45,45,5);
    }
    const auto rotateBone=[&](size_t index,const Matrix& rotation) {
        const size_t parent=bones[index].parentIndex;
        const Matrix local=parent<bones.size()?multiply(inverse(world[parent]),world[index]):world[index];
        const Matrix posed=blendNativeRotation(local,rotation,weight);
        const Matrix next=parent<bones.size()?multiply(world[parent],posed):posed;
        const Matrix change=multiply(next,inverse(world[index]));
        for(size_t child=0;child<bones.size();++child) {
            size_t ancestor=child;
            for(size_t depth=0;ancestor<bones.size()&&depth<=bones.size();++depth) {
                if(ancestor==index) {world[child]=multiply(change,world[child]);break;}
                ancestor=bones[ancestor].parentIndex;
            }
        }
    };
    const auto localOf=[&](size_t index) {
        const size_t parent=bones[index].parentIndex;
        return parent<bones.size()?multiply(inverse(world[parent]),world[index]):world[index];
    };
    if(aim->arm) {
        if(state.lowerActive)rotateBone(lower,multiply(axisRotation(1,0,0,-state.lowerPitch),axisRotation(0,0,1,-state.lowerYaw)));
        const Matrix arm=multiply(axisRotation(0,0,1,-(state.upperYaw+90)),
            multiply(axisRotation(0,1,0,-state.upperPitch),axisRotation(1,0,0,state.upperRoll)));
        rotateBone(upper,arm);
        rotateBone(hand,multiply(localOf(hand),axisRotation(1,0,0,state.handRoll)));
    }
    const auto middle=facingFrame(spine);
    const float angle=-std::atan2(float(-middle[9]),float(-middle[8]));
    const Matrix torsoRotation=multiply(axisRotation(1,0,0,state.torsoYaw),
        multiply(axisRotation(0,-std::sin(angle),std::cos(angle),state.torsoPitch),localOf(torso)));
    rotateBone(torso,torsoRotation);
}

Matrix nativeAssociationPose(const Matrix& bind,const StorylandModelBone& bone,
    const std::vector<StorylandAnimationPoseLayer>& associations,bool lockTranslation,
    const std::vector<AnimationTracks>* resolved=nullptr,const std::string* boneName=nullptr,
    bool upperBodyBone=true,const std::vector<uint16_t>* nativeRootFlags=nullptr,
    bool nativeSequenceClock=false);
bool nativeAssociationHasPoseTrack(const StorylandModelBone& bone,
    const std::vector<StorylandAnimationPoseLayer>& associations, bool lockTranslation,
    const std::vector<AnimationTracks>* resolved=nullptr,const std::string* boneName=nullptr,
    bool upperBodyBone=true);

void nativePedLook(const std::vector<StorylandModelBone>& bones,std::vector<Matrix>& frames,
    const Matrix& outputFrame,const StorylandPedLookPose* look) {
    if(!look||!look->state)return;
    auto& state=*look->state;
    if(!look->looking&&(!state.initialized||state.restored)) {state.restored=true;return;}
    const auto indexOf=[&](uint32_t id) {
        for(size_t index=0;index<bones.size();++index)if(bones[index].boneId==id)return index;
        return SIZE_MAX;
    };
    const size_t torso=indexOf(3),neck=indexOf(4),head=indexOf(5);
    if(torso==SIZE_MAX||neck==SIZE_MAX||head==SIZE_MAX)
        throw std::runtime_error("Native looking requires authored torso, neck and head bones.");
    Matrix modelWorld;
    std::copy(look->modelWorld.begin(),look->modelWorld.end(),modelWorld.begin());
    const auto worldFrame=[&](size_t index) {
        return multiply(modelWorld,multiply(outputFrame,frames[index]));
    };
    const auto localFrame=[&](size_t index) {
        const size_t parent=bones[index].parentIndex;
        return parent<bones.size()?multiply(inverse(frames[parent]),frames[index]):frames[index];
    };
    const auto normalized=[](Matrix matrix) {
        for(size_t column=0;column<3;++column) {
            const double length=std::hypot(matrix[column*4],matrix[column*4+1],matrix[column*4+2]);
            if(!std::isfinite(length)||length<1e-8)
                throw std::runtime_error("Native look reference bone has a singular rotation.");
            for(size_t row=0;row<3;++row)matrix[column*4+row]/=length;
        }
        return matrix;
    };
    constexpr float pi=3.14159265358979323846f;
    constexpr float degrees=57.29577951308232f;
    const auto wrap=[](float angle) {return std::remainder(angle,6.28318530717958647692f);};
    const Matrix reference=normalized(worldFrame(neck));
    const float referenceYaw=float(std::atan2(-reference[9],-reference[8]));
    const float referencePitch=float(std::asin(std::clamp(-reference[6],-1.0,1.0)));
    if(!state.initialized) {
        const float angle=look->modelForwardAngle/degrees;
        const double forwardX=modelWorld[0]*std::cos(angle)+modelWorld[4]*std::sin(angle);
        const double forwardY=modelWorld[1]*std::cos(angle)+modelWorld[5]*std::sin(angle);
        const float heading=float(std::atan2(-forwardX,forwardY));
        state.headYaw=wrap(referenceYaw-heading);
        state.headPitch=referencePitch;
        state.sampledAt=look->milliseconds;
        state.tickRemainder=1000;
        state.initialized=true;
    }
    const uint32_t elapsed=look->milliseconds-state.sampledAt;
    state.sampledAt=look->milliseconds;
    const uint64_t accumulated=uint64_t(state.tickRemainder)+uint64_t(std::min(elapsed,1000u))*30u;
    const unsigned ticks=unsigned(accumulated/1000u);
    state.tickRemainder=uint32_t(accumulated%1000u);
    float targetYaw=0,targetPitch=0,relativeYaw=0,relativePitch=0;
    if(look->looking) {
        const Matrix origin=worldFrame(torso);
        const auto& target=look->targetPosition;
        targetYaw=std::atan2(float(origin[12])-target[0],target[1]-float(origin[13]));
        targetPitch=std::atan2(float(origin[14])-target[2],
            std::hypot(look->actorPosition[0]-target[0],look->actorPosition[1]-target[1]));
        relativeYaw=wrap(targetYaw-referenceYaw);
        relativePitch=wrap(targetPitch-referencePitch)*std::cos(std::min(std::fabs(relativeYaw),pi*.5f));
    }
    const Matrix animatedHead=normalized(localFrame(head));
    float restoreYaw=float(std::acos(std::clamp(animatedHead[10],-1.0,1.0)));
    if(animatedHead[9]>0)restoreYaw=-restoreYaw;
    for(unsigned tick=0;tick<ticks;++tick) {
        if(look->looking) {
            const int result=storylandMoveNativeLookLimb(state.headYaw,state.headPitch,
                relativeYaw,relativePitch,StorylandNativeLookHeadLimits);
            if(result!=2&&look->allowTorso)
                storylandMoveNativeLookLimb(state.torsoYaw,state.torsoPitch,
                    relativeYaw,targetPitch,StorylandNativeLookTorsoLimits);
            state.restoreStarted=false;
        } else {
            if(!state.restoreStarted)state.restoreStarted=true;
            else state.restored=storylandMoveNativeLookLimb(state.headYaw,state.headPitch,
                restoreYaw,0,StorylandNativeLookRestoreLimits)==2;
            if(look->allowTorso)
                storylandMoveNativeLookLimb(state.torsoYaw,state.torsoPitch,0,0,
                    StorylandNativeLookTorsoLimits);
            if(state.restored)break;
        }
    }
    const auto rotate=[&](size_t index,const Matrix& rotation) {
        const auto parent=bones[index].parentIndex;
        const Matrix local=localFrame(index);
        const Matrix posed=blendNativeRotation(local,rotation,1);
        const Matrix next=parent<bones.size()?multiply(frames[parent],posed):posed;
        const Matrix change=multiply(next,inverse(frames[index]));
        for(size_t child=0;child<bones.size();++child) {
            size_t ancestor=child;
            for(size_t depth=0;ancestor<bones.size()&&depth<=bones.size();++depth) {
                if(ancestor==index) {frames[child]=multiply(change,frames[child]);break;}
                ancestor=bones[ancestor].parentIndex;
            }
        }
    };
    // SLUS_214.23 270438: pitch about Z replaces the head quaternion,
    // then yaw about X is concatenated. RestoreLook uses RotateLimb's X/Z order.
    const Matrix headYaw=axisRotation(1,0,0,state.headYaw*degrees);
    const Matrix headPitch=axisRotation(0,0,1,state.headPitch*degrees);
    rotate(head,look->looking?multiply(headPitch,headYaw):multiply(headYaw,headPitch));
    if(look->allowTorso)rotate(torso,multiply(axisRotation(1,0,0,state.torsoYaw*degrees),
        axisRotation(0,0,1,state.torsoPitch*degrees)));
}
}

static void storylandRelativeRootTranslation(Matrix& posed,const Matrix& bind,
    const StorylandAnimTrack* first,float firstSeconds,const StorylandAnimTrack* second,
    float secondSeconds,float secondWeight,bool retainFirstRootOffset) {
    const auto offset=[](const StorylandAnimTrack* track,float seconds) {
        std::array<double,3> result{};
        if(!track||track->keys.empty()||!(track->channelFlags&2u))return result;
        const auto now=sample(*track,seconds);const auto& start=track->keys.front();
        return std::array<double,3>{now.tx-start.tx,now.ty-start.ty,now.tz-start.tz};
    };
    const auto firstOffset=offset(first,firstSeconds);
    auto secondOffset=second?offset(second,secondSeconds):firstOffset;
    if(retainFirstRootOffset&&second)
        for(size_t axis=0;axis<3;++axis)secondOffset[axis]+=firstOffset[axis];
    for(size_t axis=0;axis<3;++axis)
        posed[12+axis]=bind[12+axis]+firstOffset[axis]+(secondOffset[axis]-firstOffset[axis])*secondWeight;
}

bool storylandBlendAnimationFrames(const std::vector<uint32_t>& parents,
    std::vector<std::array<float,16>>& frames,StorylandAnimationTransition& transition,std::string& error) {
    try {
        if(parents.size()!=frames.size()||!std::isfinite(transition.seconds)||
            !std::isfinite(transition.blendRate)||transition.blendRate<=0)
            throw std::runtime_error("Pose transition requires finite time, rate and matching bone parents.");
        std::vector<Matrix> current(frames.size()),local(frames.size()),world(frames.size());
        for(size_t i=0;i<frames.size();++i) {
            for(size_t cell=0;cell<16;++cell) {
                if(!std::isfinite(frames[i][cell]))throw std::runtime_error("Pose transition received a non-finite bone.");
                current[i][cell]=frames[i][cell];
            }
        }
        for(size_t i=0;i<frames.size();++i) {
            if(parents[i]!=UINT32_MAX&&parents[i]>=frames.size())throw std::runtime_error("Pose transition parent is outside the skeleton.");
            local[i]=parents[i]==UINT32_MAX?current[i]:multiply(inverse(current[parents[i]]),current[i]);
        }
        const bool ready=transition.initialized&&transition.previousLocal.size()==frames.size();
        const bool changed=ready&&(transition.appliedKey!=transition.key||transition.seconds+1e-5f<transition.lastSeconds);
        if(!ready) {transition.weight=1;transition.sourceLocal.clear();}
        else if(changed) {transition.sourceLocal=transition.previousLocal;transition.weight=0;}
        else transition.weight=std::min(1.0f,transition.weight+
            std::clamp(transition.seconds-transition.lastSeconds,0.0f,.1f)*transition.blendRate);
        transition.lastSeconds=transition.seconds;transition.appliedKey=transition.key;transition.initialized=true;
        if(transition.weight<1&&transition.sourceLocal.size()==frames.size())for(size_t i=0;i<frames.size();++i) {
            Matrix source;for(size_t cell=0;cell<16;++cell)source[cell]=transition.sourceLocal[i][cell];
            Matrix blended=blendNativeRotation(source,local[i],transition.weight);
            for(size_t axis=0;axis<3;++axis) {
                const float weight=transition.weight;
                const double sourceScale=std::hypot(source[axis*4],source[axis*4+1],source[axis*4+2]);
                const double targetScale=std::hypot(local[i][axis*4],local[i][axis*4+1],local[i][axis*4+2]);
                if(sourceScale>1e-12)for(size_t row=0;row<3;++row)
                    blended[axis*4+row]*=(sourceScale+(targetScale-sourceScale)*weight)/sourceScale;
                blended[12+axis]=source[12+axis]+(local[i][12+axis]-source[12+axis])*weight;
            }
            local[i]=blended;
        }
        transition.previousLocal.resize(frames.size());
        for(size_t i=0;i<frames.size();++i)for(size_t cell=0;cell<16;++cell)
            transition.previousLocal[i][cell]=float(local[i][cell]);
        std::vector<uint8_t> visited(frames.size(),0);
        std::function<void(size_t)> compose=[&](size_t index) {
            if(visited[index]==2)return;
            if(visited[index]==1)throw std::runtime_error("Pose transition skeleton contains a parent cycle.");
            visited[index]=1;
            if(parents[index]!=UINT32_MAX) {compose(parents[index]);world[index]=multiply(world[parents[index]],local[index]);}
            else world[index]=local[index];
            visited[index]=2;
        };
        for(size_t i=0;i<frames.size();++i) {
            compose(i);for(size_t cell=0;cell<16;++cell)frames[i][cell]=float(world[i][cell]);
        }
        if(transition.weight>=1)transition.sourceLocal.clear();
        error.clear();return true;
    } catch(const std::exception& exception) {error=exception.what();return false;}
}

static void blendAnimationWorld(const std::vector<StorylandModelBone>& bones,
    std::vector<Matrix>& world,StorylandAnimationTransition* transition) {
    if(!transition)return;
    std::vector<uint32_t> parents;std::vector<std::array<float,16>> frames(world.size());
    parents.reserve(bones.size());
    for(size_t i=0;i<bones.size();++i) {
        parents.push_back(bones[i].parentIndex);
        for(size_t cell=0;cell<16;++cell)frames[i][cell]=float(world[i][cell]);
    }
    std::string error;
    if(!storylandBlendAnimationFrames(parents,frames,*transition,error))throw std::runtime_error(error);
    for(size_t i=0;i<world.size();++i)for(size_t cell=0;cell<16;++cell)world[i][cell]=frames[i][cell];
}

static bool storylandSampleMobileAnimationBlendedImpl(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage,
    const StorylandAnimFile* secondary, float secondarySeconds, float secondaryWeight,
    const StorylandAnimFile* upper, float upperSeconds, float upperWeight,
    std::vector<std::array<float,16>>* animatedFrames=nullptr,bool nativePartialTracks=false,
    const std::vector<StorylandAnimationPoseLayer>* associations=nullptr,unsigned relativeRoot=0,
    const StorylandPedAimPose* aim=nullptr,StorylandAnimationTransition* transition=nullptr,
    const std::vector<std::array<float,16>>* suppliedFrames=nullptr,const StorylandPedLookPose* look=nullptr) {
    if(animatedFrames)animatedFrames->clear();
    try {
        if(!model.isMobileLcsDff() || !std::isfinite(seconds))throw std::runtime_error("Expected a mobile DFF and a finite animation time.");
        const auto& bones=model.armatureBones(); const auto& bytes=model.rawBytes();
        const bool upperBodyMask=associations&&std::any_of(associations->begin(),associations->end(),
            [](const auto& association){return association.upperBodyOnly;});
        std::vector<Matrix> local(bones.size()),bindWorld(bones.size()),poseWorld(bones.size()),deltas(bones.size());
        std::vector<uint8_t> state(bones.size(),0u);
        bool hasPoseTrack=false;
        for(size_t index=0;index<bones.size();++index){
            local[index]=identity();
            for(size_t vector=0;vector<4;++vector)for(size_t axis=0;axis<3;++axis)local[index][vector*4+axis]=readFloat(bytes,size_t(bones[index].offset)+vector*12u+axis*4u);
        }
        std::function<void(size_t)> compose=[&](size_t index){
            if(state[index]==2u)return;
            if(state[index]==1u)throw std::runtime_error("Mobile frame hierarchy contains a cycle.");
            state[index]=1u;
            const auto& bone=bones[index];
            const bool lock=lockRootMotion&&(bone.boneId==0u || (model.isManhuntModel() && bone.boneId==1000u));
            Matrix posed;
            if(associations) {
                const bool upperBody=!upperBodyMask||nativeWeaponUpperBodyBone(bones,index);
                hasPoseTrack=hasPoseTrack||nativeAssociationHasPoseTrack(bone,*associations,lock,nullptr,nullptr,upperBody);
                posed=nativeAssociationPose(local[index],bone,*associations,lock,nullptr,nullptr,upperBody);
            }
            else {
                const StorylandAnimTrack* matched=nullptr;
                if(bone.boneId!=0xFFFFFFFFu)for(const auto& track:animation.tracks())if(track.boneId==bone.boneId&&!track.keys.empty()){matched=&track;break;}
                if(!matched)for(const auto& track:animation.tracks())if(!track.keys.empty()&&normalizedName(track.name)==normalizedName(bone.name)){matched=&track;break;}
                const StorylandAnimTrack* other=nullptr;
                if(secondary) {
                    if(bone.boneId!=0xFFFFFFFFu)for(const auto& track:secondary->tracks())if(track.boneId==bone.boneId&&!track.keys.empty()){other=&track;break;}
                    if(!other)for(const auto& track:secondary->tracks())if(!track.keys.empty()&&normalizedName(track.name)==normalizedName(bone.name)){other=&track;break;}
                }
                const StorylandAnimTrack* upperTrack=nullptr;
                if(upper&&(nativePartialTracks||upperBodyBoneWeight(bone)>0)) {
                    if(bone.boneId!=0xFFFFFFFFu)for(const auto& track:upper->tracks())if(track.boneId==bone.boneId&&!track.keys.empty()){upperTrack=&track;break;}
                    if(!upperTrack)for(const auto& track:upper->tracks())if(!track.keys.empty()&&normalizedName(track.name)==normalizedName(bone.name)){upperTrack=&track;break;}
                }
                if(nativePartialTracks) {
                    hasPoseTrack=hasPoseTrack||activePoseTrack(upperTrack?upperTrack:matched,1.0f,lock);
                } else hasPoseTrack=hasPoseTrack||
                    activePoseTrack(matched,secondary?1.0f-secondaryWeight:1.0f,lock)||
                    activePoseTrack(other,secondaryWeight,lock)||
                    activePoseTrack(upperTrack,upperWeight*upperBodyBoneWeight(bone),lock);
                posed=(secondary||upper)?blendedPoseLocal(local[index],matched,seconds,other,secondarySeconds,secondaryWeight,lock,
                    upperTrack,upperSeconds,upperWeight*upperBodyBoneWeight(bone)):
                    (matched?poseLocal(local[index],*matched,seconds,lock):local[index]);
                if(nativePartialTracks) {
                    const auto* selected=upperTrack?upperTrack:matched;
                    posed=selected?poseLocal(local[index],*selected,upperTrack?upperSeconds:seconds,lock):local[index];
                }
                if(relativeRoot&&bone.boneId==0)
                    storylandRelativeRootTranslation(posed,local[index],matched,seconds,other,secondarySeconds,secondaryWeight,relativeRoot==2);
            }
            if(bone.parentIndex!=0xFFFFFFFFu){if(bone.parentIndex>=bones.size())throw std::runtime_error("Mobile frame parent is out of range.");compose(bone.parentIndex);bindWorld[index]=multiply(bindWorld[bone.parentIndex],local[index]);poseWorld[index]=multiply(poseWorld[bone.parentIndex],posed);}else{bindWorld[index]=local[index];poseWorld[index]=posed;}
            deltas[index]=multiply(poseWorld[index],inverse(bindWorld[index])); state[index]=2u;
        };
        for(size_t index=0;index<bones.size();++index)compose(index);
        if(!hasPoseTrack&&!suppliedFrames)throw std::runtime_error("Animation has no active authored tracks matching the model skeleton.");
        if(suppliedFrames) {
            if(suppliedFrames->size()!=bones.size())throw std::runtime_error("Native look requires a complete sampled skeleton.");
            for(size_t index=0;index<bones.size();++index)
                std::copy((*suppliedFrames)[index].begin(),(*suppliedFrames)[index].end(),poseWorld[index].begin());
        }
        nativeWeaponAim(bones,poseWorld,identity(),aim);
        blendAnimationWorld(bones,poseWorld,transition?transition:aim?aim->transition:nullptr);
        nativePedLook(bones,poseWorld,identity(),look);
        if(aim||transition||suppliedFrames)for(size_t index=0;index<bones.size();++index)
            deltas[index]=multiply(poseWorld[index],inverse(bindWorld[index]));
        animatedBones.resize(bones.size()); for(size_t index=0;index<bones.size();++index)animatedBones[index]={float(poseWorld[index][12]),float(poseWorld[index][13]),float(poseWorld[index][14])};
        if(animatedFrames) {
            animatedFrames->resize(poseWorld.size());
            for(size_t index=0;index<poseWorld.size();++index)
                for(size_t cell=0;cell<16;++cell)(*animatedFrames)[index][cell]=float(poseWorld[index][cell]);
        }
        const auto& skin=model.previewSkinWeights(); const auto& owners=model.previewFrameIndices();
        animatedPoints=sourcePoints;
        for(size_t index=0;index<sourcePoints.size();++index){
            StorylandModelPoint result{}; double total=0.0;
            if(index<skin.size()&&skin[index].valid)for(uint32_t influence=0;influence<std::min(4u,skin[index].influenceCount);++influence){const auto& item=skin[index].influences[influence];if(item.weight<=0.0f||item.boneIndex>=deltas.size())continue;const auto p=transform(deltas[item.boneIndex],sourcePoints[index]);result.x+=p.x*item.weight;result.y+=p.y*item.weight;result.z+=p.z*item.weight;
                if(p.hasNormal) {
                    result.nx+=p.nx*item.weight;result.ny+=p.ny*item.weight;result.nz+=p.nz*item.weight;
                    result.hasNormal=true;
                }
                if(p.hasPs2Normal) {
                    result.ps2Nx+=p.ps2Nx*item.weight;result.ps2Ny+=p.ps2Ny*item.weight;result.ps2Nz+=p.ps2Nz*item.weight;
                    result.hasPs2Normal=true;
                }
                total+=item.weight;}
            if(total>0.0) {
                result.x=float(result.x/total);result.y=float(result.y/total);result.z=float(result.z/total);
                const float length=std::hypot(result.nx,result.ny,result.nz);
                result.hasNormal=result.hasNormal&&std::isfinite(length)&&length>1e-6f;
                if(result.hasNormal) {result.nx/=length;result.ny/=length;result.nz/=length;}
                animatedPoints[index]=result;
            }
            else if(index<owners.size()&&owners[index]<deltas.size())animatedPoints[index]=transform(deltas[owners[index]],sourcePoints[index]);
            const auto& p=animatedPoints[index];if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Mobile animation produced a non-finite position.");
        }
        errorMessage.clear();return true;
    } catch(const std::exception& exception){errorMessage=exception.what();animatedPoints.clear();animatedBones.clear();if(animatedFrames)animatedFrames->clear();return false;}
}

bool storylandSampleMobileAnimation(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage) {
    return storylandSampleMobileAnimationBlendedImpl(model,animation,seconds,lockRootMotion,sourcePoints,
        animatedPoints,animatedBones,errorMessage,nullptr,0,0,nullptr,0,0);
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
std::vector<size_t> mirroredBoneSources(const std::vector<std::string>& names) {
    std::vector<size_t> sources(names.size());
    for(size_t index=0;index<names.size();++index) {
        sources[index]=index;
        std::string counterpart=names[index];
        if(counterpart.empty()||(counterpart[0]!='l'&&counterpart[0]!='r'))continue;
        counterpart[0]=counterpart[0]=='l'?'r':'l';
        for(size_t candidate=0;candidate<names.size();++candidate)
            if(names[candidate]==counterpart) {sources[index]=candidate;break;}
    }
    return sources;
}
std::vector<Matrix> mirroredNativePose(const std::vector<Matrix>& bindWorld,
    const std::vector<Matrix>& poseWorld,const std::vector<size_t>& sources,const Matrix& reflection) {
    std::vector<Matrix> mirroredWorld(poseWorld.size());
    for(size_t index=0;index<poseWorld.size();++index) {
        const size_t source=sources[index];
        const Matrix displacement=multiply(poseWorld[source],inverse(bindWorld[source]));
        mirroredWorld[index]=multiply(reflection,
            multiply(displacement,multiply(reflection,bindWorld[index])));
    }
    return mirroredWorld;
}
std::vector<StorylandAnimTrack> mirroredNativeAssociationTracks(const StorylandModelFile& model,
    const StorylandAnimationPoseLayer& association,bool lockRootMotion,
    const std::vector<std::string>& names,const Matrix& reflection) {
    const auto& bones=model.armatureBones();
    const auto& bytes=model.rawBytes();
    const AnimationTracks tracks(association.animation,association.tracks);
    std::vector<Matrix> bindWorld(bones.size()),poseWorld(bones.size());
    std::vector<const StorylandAnimTrack*> authored(bones.size(),nullptr);
    std::vector<uint8_t> state(bones.size(),0u);
    std::function<void(size_t)> compose=[&](size_t index) {
        if(state[index]==2u)return;
        if(state[index]==1u)throw std::runtime_error("Model skeleton contains a parent cycle.");
        state[index]=1u;
        const auto& bone=bones[index];
        const Matrix bind=nativeMatrix(bytes,size_t(bone.offset)+(model.isManhuntModel()?0x40u:0x10u));
        authored[index]=tracks.find(bone,names[index]);
        const bool lock=lockRootMotion&&(bone.boneId==0u||(model.isManhuntModel()&&bone.boneId==1000u));
        const Matrix pose=authored[index]?poseLocalKey(bind,sample(*authored[index],association.seconds),
            authored[index]->channelFlags&3u,lock):bind;
        if(bone.parentIndex!=UINT32_MAX) {
            if(bone.parentIndex>=bones.size())throw std::runtime_error("Model bone parent is out of range.");
            compose(bone.parentIndex);
            bindWorld[index]=multiply(bindWorld[bone.parentIndex],bind);
            poseWorld[index]=multiply(poseWorld[bone.parentIndex],pose);
        } else {bindWorld[index]=bind;poseWorld[index]=pose;}
        state[index]=2u;
    };
    for(size_t index=0;index<bones.size();++index)compose(index);
    const auto sources=mirroredBoneSources(names);
    const auto mirroredWorld=mirroredNativePose(bindWorld,poseWorld,sources,reflection);
    std::vector<StorylandAnimTrack> result;
    result.reserve(bones.size());
    for(size_t index=0;index<bones.size();++index) {
        const auto* source=authored[sources[index]];
        if(!source)continue;
        const auto& bone=bones[index];
        const Matrix local=bone.parentIndex==UINT32_MAX?mirroredWorld[index]:
            multiply(inverse(mirroredWorld[bone.parentIndex]),mirroredWorld[index]);
        const auto rotation=rotationQuaternion(local);
        StorylandAnimTrack track;
        track.index=uint32_t(result.size());track.boneId=bone.boneId;
        track.boneIndex=uint32_t(index);track.parentIndex=bone.parentIndex;
        track.name=bone.name;track.hasNativeBoneTag=bone.boneId!=UINT32_MAX;
        track.channelFlags=source->channelFlags;
        StorylandAnimKey key;
        key.qx=float(rotation[0]);key.qy=float(rotation[1]);
        key.qz=float(rotation[2]);key.qw=float(rotation[3]);
        key.tx=float(local[12]);key.ty=float(local[13]);key.tz=float(local[14]);
        track.keys.push_back(key);
        result.push_back(std::move(track));
    }
    return result;
}
std::vector<StorylandAnimTrack> mirroredNativeRootAssociationTracks(const StorylandModelFile& model,
    const StorylandAnimationPoseLayer& association,const std::vector<std::string>& names,
    bool nativeSequenceClock) {
    const auto& bones=model.armatureBones();
    const AnimationTracks tracks(association.animation,association.tracks);
    std::vector<StorylandAnimTrack> result;
    result.reserve(bones.size());
    for(size_t index=0;index<bones.size();++index) {
        const auto& bone=bones[index];
        uint32_t sourceId=bone.boneId;
        // VCS18E748 binds mirrored sequences to their opposite native limb.
        if((sourceId>=21&&sourceId<=25)||(sourceId>=41&&sourceId<=43))sourceId+=10;
        else if((sourceId>=31&&sourceId<=35)||(sourceId>=51&&sourceId<=53))sourceId-=10;
        const auto sourceBone=std::find_if(bones.begin(),bones.end(),
            [&](const auto& candidate){return candidate.boneId==sourceId;});
        if(sourceBone==bones.end())continue;
        const auto* source=tracks.find(*sourceBone,names[size_t(sourceBone-bones.begin())]);
        if(!source||source->keys.empty())continue;
        auto key=nativeSequenceClock?sampleNativeSequenceKey(*source,association.seconds):
            sample(*source,association.seconds);
        key.tx=-key.tx;
        // VCS2F1D38 converts root, pelvis and ordinary bone quaternions;
        // native jaw6 is excluded by its caller at280850.
        const auto original=key;
        if(sourceId==0) {
            key.qx=original.qy;key.qy=original.qx;
            key.qz=-original.qw;key.qw=-original.qz;
        } else if(sourceId==1) {
            key.qx=original.qw;key.qy=-original.qz;
            key.qz=-original.qy;key.qw=original.qx;
        } else if(source->boneId!=6) {
            key.qz=-original.qz;key.qw=-original.qw;
        }
        StorylandAnimTrack track;
        track.index=uint32_t(result.size());track.boneId=bone.boneId;
        track.boneIndex=uint32_t(index);track.parentIndex=bone.parentIndex;
        track.name=bone.name;track.hasNativeBoneTag=bone.boneId!=UINT32_MAX;
        track.channelFlags=source->channelFlags;
        track.keys.push_back(key);
        result.push_back(std::move(track));
    }
    return result;
}
bool nativeAssociationHasPoseTrack(const StorylandModelBone& bone,
    const std::vector<StorylandAnimationPoseLayer>& associations,bool lockTranslation,
    const std::vector<AnimationTracks>* resolved,const std::string* boneName,bool upperBodyBone) {
    for(size_t index=0;index<associations.size();++index) {
        const auto& association=associations[index];
        if(!association.animation||association.weight<=0)continue;
        if(association.upperBodyOnly&&!upperBodyBone)continue;
        const auto* track=resolved?(*resolved)[index].find(bone,*boneName):
            AnimationTracks(association.animation,association.tracks).find(bone,normalizedName(bone.name));
        if(activePoseTrack(track,association.weight,lockTranslation,false))return true;
    }
    return false;
}
Matrix nativeAssociationPose(const Matrix& bind,const StorylandModelBone& bone,
    const std::vector<StorylandAnimationPoseLayer>& associations,bool lockTranslation,
    const std::vector<AnimationTracks>* resolved,const std::string* boneName,
    bool upperBodyBone,const std::vector<uint16_t>* nativeRootFlags,bool nativeSequenceClock) {
    std::array<const StorylandAnimTrack*,11> tracks{};
    float partialRotationWeight=0;
    float partialTranslationWeight=0;
    for(size_t index=0;index<associations.size();++index) {
        const auto& association=associations[index];
        if(association.upperBodyOnly&&!upperBodyBone)continue;
        tracks[index]=resolved?(*resolved)[index].find(bone,*boneName):
            AnimationTracks(association.animation,association.tracks).find(bone,normalizedName(bone.name));
        if(!tracks[index]||!association.partial)continue;
        if(tracks[index]->channelFlags&1u)partialRotationWeight+=association.weight;
        if(tracks[index]->channelFlags&2u)partialTranslationWeight+=association.weight;
    }
    float rotation[4]{},translation[3]{},translationWeight=0;
    for(size_t index=0;index<associations.size();++index) {
        const auto* track=tracks[index];
        if(!track)continue;
        const auto& association=associations[index];
        if(association.weight<=0)continue;
        auto key=nativeSequenceClock?sampleNativeSequenceKey(*track,association.seconds):
            sample(*track,association.seconds);
        if(nativeRootFlags&&((*nativeRootFlags)[index]&0x40u)) {
            key.ty=0;
            if((*nativeRootFlags)[index]&0x80u)key.tx=0;
        }
        if(track->channelFlags&2u) {
            const float weight=association.partial?
                association.weight/std::max(1.0f,partialTranslationWeight):
                association.weight*(1.0f-std::min(1.0f,partialTranslationWeight));
            translationWeight+=weight;
            translation[0]+=key.tx*weight;
            translation[1]+=key.ty*weight;
            translation[2]+=key.tz*weight;
        }
        if(track->channelFlags&1u) {
            const float weight=association.partial?
                association.weight/std::max(1.0f,partialRotationWeight):
                association.weight*(1.0f-std::min(1.0f,partialRotationWeight));
            const float quaternion[]={key.qx*weight,key.qy*weight,key.qz*weight,key.qw*weight};
            float dot=0;
            for(size_t axis=0;axis<4;++axis)dot+=rotation[axis]*quaternion[axis];
            for(size_t axis=0;axis<4;++axis)
                rotation[axis]+=dot<0?-quaternion[axis]:quaternion[axis];
        }
    }
    Matrix result=bind;
    const float length=std::sqrt(rotation[0]*rotation[0]+rotation[1]*rotation[1]+
        rotation[2]*rotation[2]+rotation[3]*rotation[3]);
    if(length>0) {
        const double x=rotation[0]/length,y=rotation[1]/length,z=rotation[2]/length,w=rotation[3]/length;
        double scale[3];
        for(size_t axis=0;axis<3;++axis)
            scale[axis]=std::sqrt(bind[axis*4]*bind[axis*4]+bind[axis*4+1]*bind[axis*4+1]+bind[axis*4+2]*bind[axis*4+2]);
        result[0]=(1-2*(y*y+z*z))*scale[0];result[1]=2*(x*y+z*w)*scale[0];result[2]=2*(x*z-y*w)*scale[0];
        result[4]=2*(x*y-z*w)*scale[1];result[5]=(1-2*(x*x+z*z))*scale[1];result[6]=2*(y*z+x*w)*scale[1];
        result[8]=2*(x*z+y*w)*scale[2];result[9]=2*(y*z-x*w)*scale[2];result[10]=(1-2*(x*x+y*y))*scale[2];
    }
    if(!lockTranslation)for(size_t axis=0;axis<3;++axis) {
        const float bindWeight=nativeRootFlags?1.0f:1.0f-std::min(1.0f,translationWeight);
        result[12+axis]=translation[axis]/std::max(1.0f,translationWeight)+
            bindWeight*float(bind[12+axis]);
    }
    if(!std::all_of(result.begin(),result.end(),[](double value){return std::isfinite(value);}))
        throw std::runtime_error("Native animation associations produced a non-finite bone frame.");
    return result;
}
}

std::array<float,2> storylandNativeAnimationRootDelta(const StorylandAnimFile& animation,
    uint16_t associationFlags,float fromSeconds,float toSeconds,bool mirrored) {
    if(!(associationFlags&0x40u)||!std::isfinite(fromSeconds)||!std::isfinite(toSeconds))return {};
    const auto root=std::find_if(animation.tracks().begin(),animation.tracks().end(),
        [](const auto& track){return track.boneId==0&&(track.channelFlags&2u)&&!track.keys.empty();});
    if(root==animation.tracks().end())return {};
    const float duration=std::max(0.0f,animation.durationSeconds());
    const auto first=sampleNativeSequenceKey(*root,std::clamp(fromSeconds,0.0f,duration));
    const auto last=sampleNativeSequenceKey(*root,std::clamp(toSeconds,0.0f,duration));
    const float x=(associationFlags&0x80u)?(last.tx-first.tx)*(mirrored?-1.0f:1.0f):0.0f;
    const float y=last.ty-first.ty;
    return std::isfinite(x)&&std::isfinite(y)?std::array<float,2>{x,y}:std::array<float,2>{};
}

bool storylandNativeAnimationForwardAngle(const StorylandModelFile& model,float& degrees) {
    const auto& bytes=model.rawBytes();
    const auto& bones=model.armatureBones();
    if(model.isManhuntModel() || bones.empty() || bytes.size()<0x28u ||
        nativeU32(bytes,0)!=0x006D646Cu ||
        (model.modelKind()!=StorylandModelKind::PedModel &&
            model.modelKind()!=StorylandModelKind::CutsceneModel))return false;
    try {
        const uint32_t clump=nativeU32(bytes,0x24u);
        if(!clump || uint64_t(clump)+0x10u>bytes.size())return false;
        const uint32_t clumpType=nativeU32(bytes,clump);
        if(clumpType!=0x00000002u && clumpType!=0x0000AA02u)return false;
        const uint32_t sentinel=clump+8u;
        std::vector<uint32_t> visited;
        for(uint32_t link=nativeU32(bytes,sentinel);link && link!=sentinel;) {
            if(link<0x1Cu || uint64_t(link)+8u>bytes.size() ||
                std::find(visited.begin(),visited.end(),link)!=visited.end())return false;
            visited.push_back(link);
            const uint32_t atomic=link-0x1Cu;
            if(uint64_t(atomic)+0x34u>bytes.size())return false;
            const uint32_t tag=nativeU32(bytes,atomic);
            if((tag!=0x0004AA01u && tag!=0x0000AA01u && tag!=0x01050001u &&
                (tag&0xFFFF00FFu)!=0x00040001u) ||
                nativeU32(bytes,size_t(atomic)+0x18u)!=clump)return false;
            const uint32_t frame=nativeU32(bytes,size_t(atomic)+4u);
            const uint32_t hierarchy=nativeU32(bytes,size_t(atomic)+0x2Cu);
            if(frame && uint64_t(frame)+0x90u<=bytes.size() && hierarchy &&
                uint64_t(hierarchy)+0x38u<=bytes.size() && nativeU32(bytes,hierarchy)==0x3000u) {
                const uint32_t anchor=nativeU32(bytes,size_t(hierarchy)+0x34u);
                if(std::any_of(bones.begin(),bones.end(),[&](const auto& bone){return bone.offset==anchor;})) {
                    // Native ped clips face +Y before the sampler removes the atomic frame.
                    const Matrix outputFrame=inverse(nativeMatrix(bytes,size_t(frame)+0x50u));
                    if(std::hypot(outputFrame[4],outputFrame[5])<=1e-8)return false;
                    degrees=float(std::atan2(outputFrame[5],outputFrame[4])*57.29577951308232);
                    return std::isfinite(degrees);
                }
            }
            link=nativeU32(bytes,link);
        }
    } catch(const std::exception&) {
        return false;
    }
    return false;
}

static bool storylandSampleModelAnimationImpl(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage,
    const StorylandAnimFile* secondary, float secondarySeconds, float secondaryWeight,
    const StorylandAnimFile* upper, float upperSeconds, float upperWeight,
    std::vector<std::array<float,16>>* animatedFrames=nullptr,bool mirror=false,bool cutsceneSpace=false,
    bool nativePartialTracks=false,const std::vector<StorylandAnimationPoseLayer>* associations=nullptr,
    unsigned relativeRoot=0,const StorylandPedAimPose* aim=nullptr,StorylandAnimationTransition* transition=nullptr,
    bool staticBindPose=false,const std::vector<std::array<float,16>>* suppliedFrames=nullptr,
    const StorylandPedLookPose* look=nullptr,
    const std::vector<uint16_t>* nativeScriptRootFlags=nullptr,
    unsigned rootVerticalMode=0,bool nativeSequenceClock=false,bool nativeLcsBoneTranslations=false) {
    if(animatedFrames)animatedFrames->clear();
    if(model.isMobileLcsDff()&&associations&&std::any_of(associations->begin(),associations->end(),
        [](const auto& association){return association.mirrored&&association.weight>0;})) {
        errorMessage="Mirrored native VCS association requires a Leeds skeleton.";return false;
    }
    if(model.isMobileLcsDff())return storylandSampleMobileAnimationBlendedImpl(model,animation,seconds,lockRootMotion,
        sourcePoints,animatedPoints,animatedBones,errorMessage,secondary,secondarySeconds,secondaryWeight,upper,upperSeconds,upperWeight,animatedFrames,nativePartialTracks,associations,relativeRoot,aim,transition,suppliedFrames,look);
    try {
        const auto& bones=model.armatureBones();const auto& bytes=model.rawBytes();
        if(bytes.size()<4u || (nativeU32(bytes,0)!=0x006D646Cu && !model.isManhuntModel()) || bones.empty() || !std::isfinite(seconds))
            throw std::runtime_error("Open a Leeds model with a skeleton to preview this animation.");
        Matrix atomic=identity();size_t paletteOffset=0u;uint32_t paletteCount=0u;
        for(size_t offset=0;!model.isManhuntModel() && offset+0x34u<=bytes.size();offset+=4u){
            const uint32_t tag=nativeU32(bytes,offset);
            if(tag!=0x0004AA01u && tag!=0x0000AA01u && tag!=0x01050001u && (tag&0xFFFF00FFu)!=0x00040001u)continue;
            const uint32_t frame=nativeU32(bytes,offset+4u),geometry=nativeU32(bytes,offset+0x14u),hierarchy=nativeU32(bytes,offset+0x2Cu);
            if(!frame || uint64_t(frame)+0x90u>bytes.size() || !geometry || uint64_t(geometry)+0x20u>bytes.size() || !hierarchy || uint64_t(hierarchy)+0x38u>bytes.size())continue;
            if(nativeU32(bytes,hierarchy)!=0x3000u)continue;
            const uint32_t anchor=nativeU32(bytes,hierarchy+0x34u);
            if(std::none_of(bones.begin(),bones.end(),[&](const StorylandModelBone& bone){return bone.offset==anchor;}))continue;
            atomic=nativeMatrix(bytes,size_t(frame)+0x50u);
            const uint32_t auxiliary=nativeU32(bytes,size_t(geometry)+0x18u);
            if(auxiliary && uint64_t(auxiliary)+16u<=bytes.size()) {
                const uint32_t skinBoneCount=nativeU32(bytes,auxiliary);
                const uint32_t hierarchyBoneCount=nativeU32(bytes,size_t(hierarchy)+4u);
                if(skinBoneCount && skinBoneCount==hierarchyBoneCount) {
                    paletteOffset=nativeU32(bytes,size_t(auxiliary)+12u);
                    paletteCount=skinBoneCount;
                    if(!paletteOffset || paletteCount>256u ||
                        uint64_t(paletteOffset)+uint64_t(paletteCount)*64u>bytes.size())
                        throw std::runtime_error("The model's bone matrix table is incomplete.");
                }
            }
            break;
        }
        const Matrix inverseAtomic=inverse(atomic);
        const Matrix outputFrame=cutsceneSpace?identity():inverseAtomic;
        std::vector<std::string> boneNames;
        boneNames.reserve(bones.size());
        for(const auto& bone:bones)boneNames.push_back(normalizedName(bone.name));
        std::vector<StorylandAnimationPoseLayer> preparedAssociations;
        std::vector<std::vector<StorylandAnimTrack>> mirroredTracks;
        if(associations&&std::any_of(associations->begin(),associations->end(),
            [](const auto& association){return association.mirrored&&association.weight>0;})) {
            Matrix reflection=identity();reflection[0]=-1;
            const Matrix nativeReflection=multiply(atomic,multiply(reflection,inverseAtomic));
            preparedAssociations=*associations;
            mirroredTracks.resize(associations->size());
            for(size_t index=0;index<preparedAssociations.size();++index) {
                auto& association=preparedAssociations[index];
                if(!association.mirrored||association.weight<=0)continue;
                // Mirror this association's authored channels before mixing.
                // Missing partial tracks continue to use the unmirrored base.
                mirroredTracks[index]=nativeScriptRootFlags?
                    mirroredNativeRootAssociationTracks(model,association,boneNames,nativeSequenceClock):
                    mirroredNativeAssociationTracks(model,association,lockRootMotion,boneNames,nativeReflection);
                association.tracks=&mirroredTracks[index];association.mirrored=false;
            }
            associations=&preparedAssociations;
        }
        const AnimationTracks firstTracks(associations?nullptr:&animation);
        const AnimationTracks secondTracks(associations?nullptr:secondary);
        const AnimationTracks upperTracks(associations?nullptr:upper);
        std::vector<AnimationTracks> associationTracks;
        const bool upperBodyMask=associations&&std::any_of(associations->begin(),associations->end(),
            [](const auto& association){return association.upperBodyOnly;});
        if(associations) {
            associationTracks.reserve(associations->size());
            for(const auto& association:*associations)associationTracks.emplace_back(association.animation,association.tracks);
        }
        std::vector<Matrix> bindLocal(bones.size()),bindWorld(bones.size()),poseWorld(bones.size());
        std::vector<uint8_t> state(bones.size(),0u);
        bool hasPoseTrack=false;
        std::function<void(size_t)> compose=[&](size_t index){
            if(state[index]==2u)return;
            if(state[index]==1u)throw std::runtime_error("Model skeleton contains a parent cycle.");
            state[index]=1u;const auto& bone=bones[index];
            bindLocal[index]=nativeMatrix(bytes,size_t(bone.offset)+(model.isManhuntModel()?0x40u:0x10u));
            const bool scriptRoot=nativeScriptRootFlags&&bone.boneId==0u;
            const bool lock=(!scriptRoot&&lockRootMotion&&
                (bone.boneId==0u||(model.isManhuntModel()&&bone.boneId==1000u)))||
                (nativeLcsBoneTranslations&&bone.boneId!=0u);
            Matrix local;
            if(associations) {
                const bool upperBody=!upperBodyMask||nativeWeaponUpperBodyBone(bones,index);
                hasPoseTrack=hasPoseTrack||nativeAssociationHasPoseTrack(bone,*associations,
                    lock,&associationTracks,&boneNames[index],upperBody);
                local=nativeAssociationPose(bindLocal[index],bone,*associations,lock,&associationTracks,
                    &boneNames[index],upperBody,scriptRoot?nativeScriptRootFlags:nullptr,nativeSequenceClock);
            }
            else {
                const auto* track=firstTracks.find(bone,boneNames[index]);
                const auto* other=secondTracks.find(bone,boneNames[index]);
                const float upperMask=upper?upperBodyBoneWeight(boneNames[index]):0.0f;
                const auto* upperTrack=(upper&&(nativePartialTracks||upperMask>0))?upperTracks.find(bone,boneNames[index]):nullptr;
                if(nativePartialTracks) {
                    hasPoseTrack=hasPoseTrack||activePoseTrack(upperTrack?upperTrack:track,1.0f,lock);
                } else hasPoseTrack=hasPoseTrack||
                    activePoseTrack(track,secondary?1.0f-secondaryWeight:1.0f,lock)||
                    activePoseTrack(other,secondaryWeight,lock)||
                    activePoseTrack(upperTrack,upperWeight*upperMask,lock);
                local=(secondary||upper)?blendedPoseLocal(bindLocal[index],track,seconds,other,secondarySeconds,secondaryWeight,lock,
                    upperTrack,upperSeconds,upperWeight*upperMask):
                    (track?poseLocal(bindLocal[index],*track,seconds,lock):bindLocal[index]);
                if(nativePartialTracks) {
                    const auto* selected=upperTrack?upperTrack:track;
                    local=selected?poseLocal(bindLocal[index],*selected,upperTrack?upperSeconds:seconds,lock):bindLocal[index];
                }
                if (rootVerticalMode && lock && bone.boneId == 0u && track &&
                    (track->channelFlags & 2u)) {
                    const float firstHeight=rootVerticalMode==2u?0.0f:sample(*track,seconds).tz;
                    const float secondHeight=other&&(other->channelFlags&2u)?
                        sample(*other,secondarySeconds).tz:firstHeight;
                    local[14]=bindLocal[index][14]+firstHeight+
                        (secondHeight-firstHeight)*secondaryWeight;
                }
                if(relativeRoot&&bone.boneId==0)
                    storylandRelativeRootTranslation(local,bindLocal[index],track,seconds,other,secondarySeconds,secondaryWeight,relativeRoot==2);
            }
            if(bone.parentIndex!=0xFFFFFFFFu){
                if(bone.parentIndex>=bones.size())throw std::runtime_error("Model bone parent is out of range.");
                compose(bone.parentIndex);bindWorld[index]=multiply(bindWorld[bone.parentIndex],bindLocal[index]);poseWorld[index]=multiply(poseWorld[bone.parentIndex],local);
            }else{bindWorld[index]=bindLocal[index];poseWorld[index]=local;}
            state[index]=2u;
        };
        for(size_t index=0;index<bones.size();++index)compose(index);
        if(suppliedFrames) {
            if(suppliedFrames->size()!=bones.size())throw std::runtime_error("Native look requires a complete sampled skeleton.");
            for(size_t index=0;index<bones.size();++index) {
                Matrix supplied;
                std::copy((*suppliedFrames)[index].begin(),(*suppliedFrames)[index].end(),supplied.begin());
                poseWorld[index]=multiply(atomic,supplied);
            }
        }
        nativeWeaponAim(bones,poseWorld,outputFrame,aim);
        if(mirror) {
            // Leeds bones have different local bind axes: the pelvis's local
            // X is not the model's sideways axis. Reflecting every local
            // quaternion around X inverts the rider. Reflect each opposite
            // bone's animated displacement in the shared model frame, then
            // apply it to this bone's own bind frame. Two reflections keep
            // the resulting bone transform a proper rotation.
            Matrix reflection=identity();reflection[0]=-1;
            const Matrix nativeReflection=multiply(atomic,multiply(reflection,inverseAtomic));
            poseWorld=mirroredNativePose(bindWorld,poseWorld,mirroredBoneSources(boneNames),nativeReflection);
        }
        blendAnimationWorld(bones,poseWorld,transition?transition:aim?aim->transition:nullptr);
        nativePedLook(bones,poseWorld,outputFrame,look);
        animatedBones.resize(bones.size());
        for(size_t index=0;index<bones.size();++index)animatedBones[index]=transform(outputFrame,{float(poseWorld[index][12]),float(poseWorld[index][13]),float(poseWorld[index][14])});
        if(animatedFrames) {
            animatedFrames->resize(poseWorld.size());
            for(size_t index=0;index<poseWorld.size();++index) {
                const auto frame=multiply(outputFrame,poseWorld[index]);
                for(size_t cell=0;cell<16;++cell)(*animatedFrames)[index][cell]=float(frame[cell]);
            }
        }
        // PS2 skin indices address HAnim node slots. PSP weights already use bone indices.
        const bool jaw=std::find(boneNames.begin(),boneNames.end(),"jaw")!=boneNames.end();
        static const uint32_t withJaw[]={0,1,2,3,4,5,8,31,32,33,34,35,21,22,23,24,25,41,42,43,54,51,52,53,55};
        static const uint32_t withoutJaw[]={0,1,2,3,4,5,31,32,33,34,35,21,22,23,24,25,41,42,43,54,51,52,53,55};
        std::vector<Matrix> skinMatrices;
        std::vector<uint32_t> paletteBones;
        const bool direct=model.skinWeightsUseBoneIndices();
        const size_t inferredPaletteCount=model.modelKind()==StorylandModelKind::CutsceneModel?bones.size():(jaw?25u:24u);
        const size_t count=direct?bones.size():(paletteCount?paletteCount:inferredPaletteCount);
        skinMatrices.reserve(count);paletteBones.reserve(count);
        for(size_t index=0;index<count;++index) {
            uint32_t boneIndex=0xFFFFFFFFu;
            if(direct) boneIndex=uint32_t(index);
            else {
                for(uint32_t bone=0;bone<bones.size();++bone)
                    if(bones[bone].nodeId==index) {boneIndex=bone;break;}
                if(boneIndex==0xFFFFFFFFu&&index<(jaw?25u:24u)) {
                    const uint32_t id=jaw?withJaw[index]:withoutJaw[index];
                    for(uint32_t bone=0;bone<bones.size();++bone) if(bones[bone].boneId==id) {boneIndex=bone;break;}
                }
            }
            const size_t matrixIndex=direct&&boneIndex<bones.size()?bones[boneIndex].nodeId:index;
            const bool nativeBind=paletteOffset&&matrixIndex<paletteCount;
            Matrix bind=nativeBind?nativeMatrix(bytes,paletteOffset+matrixIndex*64u):identity();
            paletteBones.push_back(boneIndex);skinMatrices.push_back(identity());
            if(boneIndex>=bones.size()) continue;
            if(!nativeBind) bind=multiply(inverse(bindWorld[boneIndex]),atomic);
            skinMatrices.back()=multiply(outputFrame,multiply(poseWorld[boneIndex],bind));
        }
        animatedPoints=sourcePoints;
        if(cutsceneSpace)for(auto& point:animatedPoints)point=transform(atomic,point);
        const auto& skin=model.previewSkinWeights();
        size_t skinnedVertices=0;
        uint32_t rigidBone=UINT32_MAX;
        bool rigidBinding=true;
        for(size_t index=0;index<sourcePoints.size() && index<skin.size();++index){
            const auto& weights=skin[index];if(!weights.valid){rigidBinding=false;continue;}
            StorylandModelPoint result{};double total=0.0;
            for(uint32_t influence=0;influence<std::min(4u,weights.influenceCount);++influence){
                const auto& item=weights.influences[influence];
                if(item.weight<=0.0f)continue;
                if(item.boneIndex>=skinMatrices.size() || paletteBones[item.boneIndex]==0xFFFFFFFFu)
                    throw std::runtime_error("Vertex "+std::to_string(index)+" references unresolved skin palette entry "+
                        std::to_string(item.boneIndex)+" of "+std::to_string(skinMatrices.size())+
                        " (native palette "+std::to_string(paletteCount)+", direct="+std::to_string(direct)+
                        ", skeleton="+std::to_string(bones.size())+").");
                const uint32_t boundBone=paletteBones[item.boneIndex];
                if(rigidBone==UINT32_MAX)rigidBone=boundBone;
                else if(rigidBone!=boundBone)rigidBinding=false;
                const auto p=transform(skinMatrices[item.boneIndex],sourcePoints[index]);
                result.x+=p.x*item.weight;result.y+=p.y*item.weight;result.z+=p.z*item.weight;
                if(p.hasNormal) {
                    result.nx+=p.nx*item.weight;result.ny+=p.ny*item.weight;result.nz+=p.nz*item.weight;
                    result.hasNormal=true;
                }
                if(p.hasPs2Normal) {
                    result.ps2Nx+=p.ps2Nx*item.weight;result.ps2Ny+=p.ps2Ny*item.weight;result.ps2Nz+=p.ps2Nz*item.weight;
                    result.hasPs2Normal=true;
                }
                total+=item.weight;
            }
            if(total>0.0) {
                ++skinnedVertices;
                result.x=float(result.x/total);result.y=float(result.y/total);result.z=float(result.z/total);
                const float length=std::hypot(result.nx,result.ny,result.nz);
                result.hasNormal=result.hasNormal&&std::isfinite(length)&&length>1e-6f;
                if(result.hasNormal) {result.nx/=length;result.ny/=length;result.nz/=length;}
                animatedPoints[index]=result;
            }
            const auto& p=animatedPoints[index];if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Animation produced an invalid vertex position.");
        }
        const bool requiresSkin=model.modelKind()==StorylandModelKind::PedModel||
            model.modelKind()==StorylandModelKind::CutsceneModel;
        if(!sourcePoints.empty()&&(requiresSkin||paletteCount>0||direct)&&skinnedVertices==0)
            throw std::runtime_error("The skinned model has no vertices bound to its native skeleton.");
        const bool staticCutsceneProp=cutsceneSpace&&!sourcePoints.empty()&&
            skinnedVertices==sourcePoints.size()&&rigidBinding&&rigidBone!=UINT32_MAX;
        if(!hasPoseTrack&&!staticCutsceneProp&&!staticBindPose)
            throw std::runtime_error("Animation has no active authored tracks matching the model skeleton.");
        errorMessage.clear();return true;
    }catch(const std::exception& exception){errorMessage=exception.what();animatedPoints.clear();animatedBones.clear();if(animatedFrames)animatedFrames->clear();return false;}
}

bool storylandSampleModelAnimation(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage) {
    return storylandSampleModelAnimationImpl(model,animation,seconds,lockRootMotion,sourcePoints,
        animatedPoints,animatedBones,errorMessage,nullptr,0,0,nullptr,0,0);
}

bool storylandSampleNativeRootAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,
    const std::vector<uint16_t>& nativeFlags,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames,StorylandAnimationTransition* transition,
    StorylandNativeRootPose pose) {
    if(frames)frames->clear();
    if(model.isMobileLcsDff()||model.isManhuntModel()||
        model.modelKind()!=StorylandModelKind::PedModel||associations.empty()||
        associations.size()>11||nativeFlags.size()!=associations.size()) {
        error="Native root pose requires an original pedestrian model and matching association flags.";
        return false;
    }
    for(const auto& association:associations)
        if(!association.animation||(!association.tracks&&!association.animation->hasDecodedMotion())||
            (association.tracks&&association.tracks->empty())||
            !std::isfinite(association.seconds)||!std::isfinite(association.weight)||
            association.weight<0||association.weight>1) {
            error="Native root pose requires decoded associations, finite times and bounded weights.";
            return false;
        }
    // VCS155338 and LCS140070 retain authored Z and subtract flagged X/Y.
    // The preview retains its bind origin, unlike native19B5E8/17C8B0;
    // add the sampled displacement in that basis before both skinning and bone queries.
    const auto& first=associations.front();
    return storylandSampleModelAnimationImpl(model,*first.animation,first.seconds,true,
        sourcePoints,points,bones,error,nullptr,0,0,nullptr,0,0,frames,false,false,false,
        &associations,0,nullptr,transition,false,nullptr,nullptr,&nativeFlags,0,
        pose!=StorylandNativeRootPose::Script,pose==StorylandNativeRootPose::LcsPedestrian);
}

bool storylandSampleVcsScriptAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,
    const std::vector<uint16_t>& nativeFlags,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames,StorylandAnimationTransition* transition) {
    return storylandSampleNativeRootAssociations(model,associations,nativeFlags,
        sourcePoints,points,bones,error,frames,transition);
}

bool storylandSampleVcsPedEntityPose(const StorylandModelFile& model,
    const StorylandAnimFile& animation,float seconds,
    std::vector<StorylandNativePedBoneFrame>& frames,std::string& error) {
    frames.clear();
    try {
        const auto& bytes=model.rawBytes();
        const auto range=[&](size_t offset,size_t size) {
            return offset<=bytes.size()&&size<=bytes.size()-offset;
        };
        if(model.isMobileLcsDff()||!range(0,0x28)||nativeU32(bytes,0)!=0x006D646Cu||
            !std::isfinite(seconds))
            throw std::runtime_error("Native VCS pedestrian pose requires a finite time and an original Leeds model.");
        const uint32_t clump=nativeU32(bytes,0x24);
        if(!clump||!range(clump,0x0C))
            throw std::runtime_error("Native pedestrian clump is unavailable.");
        const uint32_t atomicLink=nativeU32(bytes,size_t(clump)+8);
        if(atomicLink<0x1C||!range(size_t(atomicLink)-0x1C,0x30))
            throw std::runtime_error("Native pedestrian skinned atomic is unavailable.");
        const size_t atomic=size_t(atomicLink)-0x1C;
        const uint32_t geometry=nativeU32(bytes,atomic+0x14);
        const uint32_t hierarchy=nativeU32(bytes,atomic+0x2C);
        if(!geometry||!range(geometry,0x1C)||!hierarchy||!range(hierarchy,0x38)||
            nativeU32(bytes,hierarchy)!=0x3000)
            throw std::runtime_error("Native pedestrian geometry or HAnim hierarchy is incomplete.");
        const uint32_t count=nativeU32(bytes,size_t(hierarchy)+4);
        const uint32_t information=nativeU32(bytes,size_t(hierarchy)+0x30);
        const uint32_t skin=nativeU32(bytes,size_t(geometry)+0x18);
        if(count==0||count>256||!information||!range(information,size_t(count)*8)||
            !skin||!range(skin,16)||nativeU32(bytes,skin)!=count)
            throw std::runtime_error("Native pedestrian skin and HAnim node counts disagree.");
        const uint32_t palette=nativeU32(bytes,size_t(skin)+12);
        if(!palette||!range(palette,size_t(count)*64))
            throw std::runtime_error("Native pedestrian inverse-bind palette is incomplete.");

        std::vector<Matrix> inverseBinds(count);
        for(size_t index=0;index<count;++index) {
            inverseBinds[index]=identity();
            for(size_t column=0;column<4;++column)for(size_t row=0;row<3;++row)
                inverseBinds[index][column*4+row]=readFloat(bytes,size_t(palette)+index*64+column*16+row*4);
        }
        std::vector<Matrix> nativeFrames(count);
        std::vector<int> parents;
        int parent=-1;
        size_t matchedTracks=0;
        frames.reserve(count);
        for(size_t index=0;index<count;++index) {
            const size_t node=size_t(information)+index*8;
            const uint32_t tag=bytes[node];
            const unsigned flags=bytes[node+2]&3u;
            if(bytes[node+1]>=count||(index==0&&(tag!=0||flags!=0)))
                throw std::runtime_error("Native pedestrian HAnim root or node index is invalid.");
            Matrix local=identity();
            if(index!=0) {
                if(parent<0||size_t(parent)>=index)
                    throw std::runtime_error("Native pedestrian HAnim parent is unavailable.");
                const auto& child=inverseBinds[index];
                std::array<float,3> childOrigin{};
                for(size_t row=0;row<3;++row)
                    childOrigin[row]=float(-(child[row*4]*child[12]+child[row*4+1]*child[13]+
                        child[row*4+2]*child[14]));
                const auto& parentBind=inverseBinds[size_t(parent)];
                for(size_t row=0;row<3;++row)
                    local[12+row]=float(parentBind[row]*childOrigin[0]+parentBind[4+row]*childOrigin[1]+
                        parentBind[8+row]*childOrigin[2]+parentBind[12+row]);
            }
            const auto track=std::find_if(animation.tracks().begin(),animation.tracks().end(),
                [tag](const auto& value) {
                    return (value.tag&0xFFFFu)==tag&&!value.keys.empty();
                });
            if(track!=animation.tracks().end()) {
                ++matchedTracks;
                if((track->channelFlags&7u)!=1u&&(track->channelFlags&7u)!=3u)
                    throw std::runtime_error("Native pedestrian association has an unsupported channel layout.");
                auto key=sample(*track,seconds);
                const double length=std::sqrt(double(key.qx)*key.qx+double(key.qy)*key.qy+
                    double(key.qz)*key.qz+double(key.qw)*key.qw);
                if(!std::isfinite(length)||length<=0)
                    throw std::runtime_error("Native pedestrian association quaternion is invalid.");
                key.qx=float(key.qx/length);key.qy=float(key.qy/length);
                key.qz=float(key.qz/length);key.qw=float(key.qw/length);
                local=poseLocalKey(local,key,track->channelFlags,false);
            }
            nativeFrames[index]=parent<0?local:multiply(nativeFrames[size_t(parent)],local);
            StorylandNativePedBoneFrame output;
            output.tag=tag;
            for(size_t component=0;component<16;++component) {
                output.matrix[component]=float(nativeFrames[index][component]);
                if(!std::isfinite(output.matrix[component]))
                    throw std::runtime_error("Native pedestrian pose produced a nonfinite matrix.");
                nativeFrames[index][component]=output.matrix[component];
            }
            frames.push_back(output);
            if(flags&2u)parents.push_back(parent);
            parent=int(index);
            if(flags&1u) {
                if(parents.empty()) {
                    if(index+1!=count)
                        throw std::runtime_error("Native pedestrian HAnim parent stack underflowed.");
                    parent=-1;
                } else {
                    parent=parents.back();parents.pop_back();
                }
            }
        }
        if(matchedTracks==0)throw std::runtime_error("Native pedestrian association has no matching original HAnim tags.");
        if(!parents.empty())throw std::runtime_error("Native pedestrian HAnim has unfinished branches.");
        error.clear();return true;
    } catch(const std::exception& exception) {
        frames.clear();error=exception.what();return false;
    }
}

bool storylandApplyNativePedLook(const StorylandModelFile& model,const StorylandPedLookPose& look,
    std::vector<std::array<float,16>>& frames,std::vector<StorylandModelPoint>& points,
    std::string& error,bool skinGeometry) {
    if(!look.state||(!look.looking&&(!look.state->initialized||look.state->restored)))return true;
    const float angles[]={look.modelForwardAngle,look.state->headYaw,look.state->headPitch,
        look.state->torsoYaw,look.state->torsoPitch};
    const auto finite=[](float value){return std::isfinite(value);};
    if(!std::all_of(std::begin(angles),std::end(angles),finite)||
        !std::all_of(look.modelWorld.begin(),look.modelWorld.end(),finite)||
        !std::all_of(look.actorPosition.begin(),look.actorPosition.end(),finite)||
        !std::all_of(look.targetPosition.begin(),look.targetPosition.end(),finite)||
        frames.size()!=model.armatureBones().size()||
        std::any_of(frames.begin(),frames.end(),[&](const auto& frame) {
            return !std::all_of(frame.begin(),frame.end(),finite);
        })) {
        error="Native looking requires a finite complete sampled skeleton and target.";return false;
    }
    const auto originalFrames=frames;
    const std::vector<StorylandModelPoint> noGeometry;
    std::vector<StorylandModelPoint> bones;
    StorylandAnimFile unusedAnimation;
    return storylandSampleModelAnimationImpl(model,unusedAnimation,0,true,
        skinGeometry?model.previewPoints():noGeometry,points,bones,error,
        nullptr,0,0,nullptr,0,0,&frames,false,false,false,nullptr,0,nullptr,nullptr,
        true,&originalFrames,&look);
}

bool storylandSampleLcsAnimationAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,std::vector<std::array<float,16>>* frames) {
    if(frames)frames->clear();
    if(associations.empty()||associations.size()>11) {
        error="Native animation sampling requires between one and eleven associations.";return false;
    }
    for(const auto& association:associations)
        if(!association.animation||(!association.tracks&&!association.animation->hasDecodedMotion())||
            (association.tracks&&association.tracks->empty())||
            !std::isfinite(association.seconds)||!std::isfinite(association.weight)||
            association.weight<0||association.weight>1) {
            error="Native animation associations require decoded clips, finite times and weights between zero and one.";
            return false;
        }
    const auto& first=associations.front();
    return storylandSampleModelAnimationImpl(model,*first.animation,first.seconds,lockRootMotion,
        sourcePoints,points,bones,error,nullptr,0,0,nullptr,0,0,frames,false,false,false,&associations);
}

bool storylandSampleNativeWeaponAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,const StorylandPedAimPose& aim,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,std::vector<std::array<float,16>>* frames) {
    if(frames)frames->clear();
    if(associations.empty()||associations.size()>11||!std::isfinite(aim.yaw)||!std::isfinite(aim.pitch)||
        !std::isfinite(aim.seconds)||aim.seconds<0||!std::isfinite(aim.modelForwardAngle)||
        !std::isfinite(aim.weight)||aim.weight<0||aim.weight>1) {
        error="Native weapon animation requires finite aim parameters and decoded associations.";return false;
    }
    for(const auto& association:associations)
        if(!association.animation||(!association.tracks&&!association.animation->hasDecodedMotion())||
            (association.tracks&&association.tracks->empty())||
            !std::isfinite(association.seconds)||!std::isfinite(association.weight)||
            association.weight<0||association.weight>1) {
            error="Native weapon animation has an invalid association.";return false;
        }
    const auto& first=associations.front();
    return storylandSampleModelAnimationImpl(model,*first.animation,first.seconds,true,sourcePoints,
        points,bones,error,nullptr,0,0,nullptr,0,0,frames,false,false,false,&associations,0,&aim);
}

bool storylandSampleModelAnimationPartial(const StorylandModelFile& model,
    const StorylandAnimFile& baseAnimation,float baseSeconds,
    const StorylandAnimFile& partialAnimation,float partialSeconds,bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,std::vector<std::array<float,16>>* frames) {
    if(!std::isfinite(partialSeconds)) {error="Partial animation time must be finite.";return false;}
    return storylandSampleModelAnimationImpl(model,baseAnimation,baseSeconds,lockRootMotion,
        sourcePoints,points,bones,error,nullptr,0,0,&partialAnimation,partialSeconds,1,frames,false,false,true);
}

bool storylandSampleModelAnimationBlended(const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation,float firstSeconds,
    const StorylandAnimFile& secondAnimation,float secondSeconds,float secondWeight,
    bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage) {
    if (!std::isfinite(secondWeight) || secondWeight < 0 || secondWeight > 1) {
        errorMessage="Animation blend weight must be between 0 and 1.";
        return false;
    }
    return storylandSampleModelAnimationImpl(model,firstAnimation,firstSeconds,lockRootMotion,
        sourcePoints,animatedPoints,animatedBones,errorMessage,&secondAnimation,secondSeconds,secondWeight,nullptr,0,0);
}

bool storylandSampleModelAnimationLayeredAim(const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation,float firstSeconds,
    const StorylandAnimFile& secondAnimation,float secondSeconds,float secondWeight,
    const StorylandAnimFile* upperAnimation,float upperSeconds,float upperWeight,
    bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage,std::vector<std::array<float,16>>* animatedFrames,
    StorylandAnimationTransition* transition,bool preserveRootVertical,const StorylandPedAimPose* aim) {
    if(!std::isfinite(secondWeight)||secondWeight<0||secondWeight>1||
       !std::isfinite(upperWeight)||upperWeight<0||upperWeight>1) {
        errorMessage="Animation layer weights must be between 0 and 1.";
        return false;
    }
    return storylandSampleModelAnimationImpl(model,firstAnimation,firstSeconds,lockRootMotion,
        sourcePoints,animatedPoints,animatedBones,errorMessage,&secondAnimation,secondSeconds,secondWeight,
        upperAnimation,upperSeconds,upperWeight,animatedFrames,false,false,false,nullptr,0,aim,
        transition,false,nullptr,nullptr,nullptr,preserveRootVertical);
}

bool storylandSampleModelAnimationLayered(const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation,float firstSeconds,
    const StorylandAnimFile& secondAnimation,float secondSeconds,float secondWeight,
    const StorylandAnimFile* upperAnimation,float upperSeconds,float upperWeight,
    bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage,std::vector<std::array<float,16>>* animatedFrames,
    StorylandAnimationTransition* transition,bool preserveRootVertical) {
    return storylandSampleModelAnimationLayeredAim(model,firstAnimation,firstSeconds,secondAnimation,
        secondSeconds,secondWeight,upperAnimation,upperSeconds,upperWeight,lockRootMotion,sourcePoints,
        animatedPoints,animatedBones,errorMessage,animatedFrames,transition,preserveRootVertical,nullptr);
}

bool storylandSampleRetargetedCrouchTransitionAim(const StorylandModelFile& model,
    const StorylandAnimFile& standing,const StorylandAnimFile& crouch,float seconds,
    float weight,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::string& error,
    std::vector<std::array<float,16>>* frames,const StorylandAnimFile* upperAnimation,
    float upperSeconds,float upperWeight,const StorylandPedAimPose* aim) {
    if(!std::isfinite(weight)||weight<0||weight>1) {
        error="Retargeted crouch blend is outside 0..1.";return false;
    }
    std::vector<StorylandModelPoint> bones;
    return storylandSampleModelAnimationImpl(model,standing,0,true,sourcePoints,
        animatedPoints,bones,error,&crouch,seconds,weight,upperAnimation,upperSeconds,upperWeight,frames,
        false,false,false,nullptr,0,aim,nullptr,false,nullptr,nullptr,nullptr,2u);
}

bool storylandSampleRetargetedCrouchTransition(const StorylandModelFile& model,
    const StorylandAnimFile& standing,const StorylandAnimFile& crouch,float seconds,
    float weight,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::string& error,
    std::vector<std::array<float,16>>* frames) {
    return storylandSampleRetargetedCrouchTransitionAim(model,standing,crouch,seconds,weight,
        sourcePoints,animatedPoints,error,frames,nullptr,0,0,nullptr);
}

bool storylandPlantRetargetedCrouchFeet(const StorylandModelFile& model,
    std::vector<StorylandModelPoint>& points,
    std::vector<std::array<float,16>>* frames,std::string& error) {
    const auto& bind=model.previewPoints();
    const auto& skin=model.previewSkinWeights();
    if(points.size()!=bind.size()||skin.size()!=bind.size()) {
        error="Retargeted crouch support requires the complete skinned model.";return false;
    }
    std::vector<uint32_t> feet;
    for(const auto& bone:model.armatureBones()) {
        const auto name=normalizedName(bone.name);
        if(bone.boneId!=43u&&bone.boneId!=53u&&bone.boneId!=54u&&bone.boneId!=55u&&
            bone.boneId!=2000u&&bone.boneId!=2001u&&name.find("foot")==std::string::npos&&
            name.find("toe")==std::string::npos)continue;
        const uint32_t palette=model.skinWeightsUseBoneIndices()?bone.index:bone.nodeId;
        if(palette!=UINT32_MAX)feet.push_back(palette);
    }
    double bindSupport=std::numeric_limits<double>::infinity();
    double poseSupport=std::numeric_limits<double>::infinity();
    for(size_t vertex=0;vertex<skin.size();++vertex) {
        const auto& weights=skin[vertex];
        if(!weights.valid)continue;
        double total=0,footWeight=0;
        for(unsigned index=0;index<std::min(4u,weights.influenceCount);++index) {
            const auto& influence=weights.influences[index];
            if(influence.weight<=0)continue;
            total+=influence.weight;
            if(std::find(feet.begin(),feet.end(),influence.boneIndex)!=feet.end())
                footWeight+=influence.weight;
        }
        if(total<=0||footWeight*2<total)continue;
        bindSupport=std::min(bindSupport,double(bind[vertex].z));
        poseSupport=std::min(poseSupport,double(points[vertex].z));
    }
    if(!std::isfinite(bindSupport)||!std::isfinite(poseSupport)) {
        error="Retargeted crouch support could not resolve native foot skin influences.";return false;
    }
    const float rootCorrection=float(bindSupport-poseSupport);
    for(auto& point:points)point.z+=rootCorrection;
    if(frames)for(auto& frame:*frames)frame[14]+=rootCorrection;
    return true;
}


bool storylandSampleModelAnimationLayered(const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation, float firstSeconds,
    const StorylandAnimFile& secondAnimation, float secondSeconds, float secondWeight,
    const StorylandAnimFile* upperAnimation, float upperSeconds, float upperWeight,
    bool lockRootMotion, const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints, std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage) {
    return storylandSampleModelAnimationLayered(model, firstAnimation, firstSeconds,
        secondAnimation, secondSeconds, secondWeight, upperAnimation, upperSeconds, upperWeight,
        lockRootMotion, sourcePoints, animatedPoints, animatedBones, errorMessage, nullptr);
}

bool storylandSampleMirroredModelAnimation(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,bool lockRootMotion,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& points,std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames,StorylandAnimationTransition* transition) {
    if(model.isMobileLcsDff()) {error="Mirrored native VCS association requires a Leeds skeleton.";return false;}
    return storylandSampleModelAnimationImpl(model,animation,seconds,lockRootMotion,sourcePoints,points,bones,error,
        nullptr,0,0,nullptr,0,0,frames,true,false,false,nullptr,0,nullptr,transition);
}

static uint32_t storylandNativeFrameNameHash(const std::string& name) {
    uint32_t hash = 0xFFFFFFFFu;
    for (unsigned char character : name) {
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        hash ^= character;
        for (unsigned bit = 0; bit < 8; ++bit)
            hash = (hash >> 1) ^ ((hash & 1u) ? 0xEDB88320u : 0u);
    }
    return hash;
}

static bool storylandNativeCutsceneHasRigidGeometry(const StorylandModelFile& model) {
    const auto& bytes = model.rawBytes();
    constexpr uint32_t clump = 0x20;
    if (bytes.size() < clump + 0x1C) return false;
    const uint32_t tag = nativeU32(bytes, clump);
    if (tag != 0x0000AA02u && tag != 0x00000002u) return false;
    const uint32_t head = clump + 8;
    const uint32_t link = nativeU32(bytes, head);
    if (link == head || link < 0x1C || uint64_t(link) + 8 > bytes.size()) return false;
    const uint32_t atomic = link - 0x1C;
    if (nativeU32(bytes, size_t(atomic) + 0x18) != clump) return false;
    const uint32_t geometry = nativeU32(bytes, size_t(atomic) + 0x14);
    if (!geometry || uint64_t(geometry) + 0x1C > bytes.size()) return false;
    // The original clump skin predicate reads its first atomic's geometry+0x18.
    // Missing decoded influences alone do not make a skinned asset rigid.
    return nativeU32(bytes, size_t(geometry) + 0x18) == 0;
}

static bool storylandSampleNativeCutsceneFrames(const StorylandModelFile& model,
    const StorylandAnimFile& animation, float seconds,
    const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints, std::vector<StorylandModelPoint>& animatedBones,
    std::string& error, std::vector<StorylandCutsceneFramePose>* framePoses, bool staticBindPose, bool replaceRigidClumpRoot) {
    if(framePoses)framePoses->clear();
    try {
        const auto& bytes = model.rawBytes();
        const auto& owners = model.previewNativeFrameOffsets();
        if (!std::isfinite(seconds) || owners.size() != sourcePoints.size() || owners.empty())
            throw std::runtime_error("The native cutscene model has no complete vertex frame bindings.");
        struct Frame {
            Matrix bind = identity();
            Matrix posed = identity();
            Matrix vertex = identity();
            StorylandModelBone bone;
            uint8_t state = 0;
        };
        std::map<uint32_t, Frame> frames;
        const AnimationTracks tracks(&animation);
        const bool nativeRigidAnimation = !animation.isIfp() && !staticBindPose;
        std::map<uint32_t, const StorylandAnimTrack*> nativeBindings;
        std::vector<uint32_t> nativeFrameOrder;
        bool hasPoseTrack = false;
        const auto readName = [&](uint32_t pointer) {
            std::string name;
            if (!pointer || pointer >= bytes.size()) return name;
            for (size_t offset = pointer; offset < bytes.size() && name.size() < 96; ++offset) {
                const uint8_t character = bytes[offset];
                if (!character) return name;
                if (character < 32 || character >= 127) return std::string{};
                name.push_back(char(character));
            }
            return std::string{};
        };
        const auto validateFrame = [&](uint32_t offset) {
            if (!offset || uint64_t(offset) + 0xAC > bytes.size())
                throw std::runtime_error("The native cutscene frame pointer is outside the model.");
            const uint32_t tag = nativeU32(bytes, offset);
            const bool node = tag == 0x0180AA00u || tag == 0x0003AA00u || tag == 0x0C000000u ||
                (tag & 0xFF0000FFu) == 0x01000000u || tag == 0x0380AA00u ||
                tag == 0x0003AA80u || tag == 0x0380AA80u ||
                (tag & 0xFF0000FFu) == 0x03000000u || tag == 0x0003AA01u ||
                tag == 0x0003AA02u || tag == 0x0380AA01u || tag == 0x0380AA02u;
            if (!node) throw std::runtime_error("The native cutscene frame has an invalid node tag.");
        };
        if (nativeRigidAnimation) {
            const uint32_t root = nativeU32(bytes, 0x24);
            validateFrame(root);
            std::map<uint32_t, uint32_t> framesByHash;
            std::function<void(uint32_t, uint32_t)> collect = [&](uint32_t offset, uint32_t parent) {
                validateFrame(offset);
                if (nativeFrameOrder.size() >= 512 ||
                    std::find(nativeFrameOrder.begin(), nativeFrameOrder.end(), offset) != nativeFrameOrder.end())
                    throw std::runtime_error("The native cutscene child hierarchy contains a cycle.");
                if (nativeU32(bytes, size_t(offset) + 4) != parent)
                    throw std::runtime_error("The native cutscene child and parent frame links disagree.");
                nativeFrameOrder.push_back(offset);
                if (parent) {
                    // Native non-skinned clumps exclude the dummy root and
                    // initialize every frame's HAnim ID to -1. Name lookup
                    // retains the last complete CRC match in frame order.
                    const uint32_t pointer = nativeU32(bytes, size_t(offset) + 0xA8);
                    if (pointer >= bytes.size())
                        throw std::runtime_error("The native cutscene frame name is outside the model.");
                    framesByHash[storylandNativeFrameNameHash(readName(pointer))] = offset;
                }
                for (uint32_t child = nativeU32(bytes, size_t(offset) + 0x90); child;) {
                    collect(child, offset);
                    child = nativeU32(bytes, size_t(child) + 0x94);
                }
            };
            collect(root, 0);
            for (uint32_t owner : owners)
                if (std::find(nativeFrameOrder.begin(), nativeFrameOrder.end(), owner) == nativeFrameOrder.end())
                    throw std::runtime_error("The native cutscene vertex frame is outside its clump hierarchy.");
            for (const auto& track : animation.tracks()) {
                // VCS association Init uses the channel's low-byte HAnim ID
                // when bit 0x8 is set; it cannot match a rigid frame's -1.
                if ((track.channelFlags & 8u) || track.keys.empty()) continue;
                const auto target = framesByHash.find(track.tag);
                if (target != framesByHash.end()) nativeBindings[target->second] = &track;
            }
        }
        std::function<void(uint32_t)> compose = [&](uint32_t offset) {
            auto& frame = frames[offset];
            if (frame.state == 2) return;
            if (frame.state == 1)
                throw std::runtime_error("The native cutscene frame hierarchy contains a cycle.");
            validateFrame(offset);
            if (frames.size() > 512)
                throw std::runtime_error("The native cutscene frame hierarchy exceeds its supported size.");
            frame.state = 1;
            StorylandModelBone bone;
            bone.boneId = nativeRigidAnimation ? 0xFFFFFFFFu : nativeU32(bytes, size_t(offset) + 0x9C);
            bone.name = readName(nativeU32(bytes, size_t(offset) + 0xA8));
            if (bone.name.empty() && !nativeRigidAnimation)
                bone.name = readName(nativeU32(bytes, size_t(offset) + 0xA4));
            frame.bone = bone;
            const auto nativeTrack = nativeBindings.find(offset);
            const auto* track = nativeRigidAnimation ?
                (nativeTrack == nativeBindings.end() ? nullptr : nativeTrack->second) :
                tracks.find(bone, normalizedName(bone.name));
            hasPoseTrack = hasPoseTrack || activePoseTrack(track, 1, false);
            const Matrix local = nativeMatrix(bytes, size_t(offset) + 0x10);
            const uint32_t parent = nativeU32(bytes, size_t(offset) + 4);
            Matrix posed = track ? poseLocal(local, *track, seconds, false) : local;
            if (nativeRigidAnimation && parent) {
                // Native rigid frame updates normalize a zero accumulator to
                // identity and add the saved bind translation. An unbound
                // channel is valid and contributes no invented motion.
                posed = track ? poseLocalKey(identity(), sample(*track, seconds),
                    track->channelFlags & 3u, false) : identity();
                for (size_t axis = 0; axis < 3; ++axis) posed[12 + axis] += local[12 + axis];
            }
            if (parent) {
                compose(parent);
                frame.bind = multiply(frames.at(parent).bind, local);
                frame.posed = multiply(frames.at(parent).posed, posed);
            } else {
                frame.bind = local;
                // SetupCutsceneToStart places the clump directly at the scene
                // origin plus the first IFP root translation. Its entity frame
                // replaces the model's loader-space root basis; retaining that
                // basis rotates world motion and tips rigid cutscene vehicles.
                frame.posed = replaceRigidClumpRoot && animation.isIfp() && !staticBindPose && !track ?
                    identity() : posed;
            }
            frame.vertex = model.previewPointsUseNativeFrames() ?
                multiply(frame.posed, inverse(frame.bind)) : frame.posed;
            frame.state = 2;
        };
        for (uint32_t offset : nativeFrameOrder) compose(offset);
        for (uint32_t owner : owners) compose(owner);
        if (!hasPoseTrack && !staticBindPose && !nativeRigidAnimation)
            throw std::runtime_error("The native cutscene animation has no authored tracks matching its frames.");
        animatedPoints = sourcePoints;
        for (size_t index = 0; index < animatedPoints.size(); ++index) {
            auto point = transform(frames.at(owners[index]).vertex, sourcePoints[index]);
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                throw std::runtime_error("The native cutscene animation produced a non-finite vertex.");
            if (point.hasNormal) {
                const float length = std::hypot(point.nx, point.ny, point.nz);
                point.hasNormal = std::isfinite(length) && length > 0.000001f;
                if (point.hasNormal) { point.nx /= length; point.ny /= length; point.nz /= length; }
            }
            animatedPoints[index] = point;
        }
        animatedBones.clear();
        for (const auto& item : frames) {
            animatedBones.push_back({float(item.second.posed[12]),
                float(item.second.posed[13]), float(item.second.posed[14])});
            if(framePoses) {
                StorylandCutsceneFramePose pose;
                pose.offset=item.first;pose.boneId=item.second.bone.boneId;pose.name=item.second.bone.name;
                for(size_t cell=0;cell<16;++cell)pose.matrix[cell]=float(item.second.posed[cell]);
                framePoses->push_back(std::move(pose));
            }
        }
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        animatedPoints.clear();
        animatedBones.clear();
        if(framePoses)framePoses->clear();
        error = exception.what();
        return false;
    }
}

bool storylandSampleCutsceneModelAnimation(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage,
    std::vector<StorylandCutsceneFramePose>* frames,bool staticBindPose,bool replaceRigidClumpRoot) {
    if(frames)frames->clear();
    const auto& skin = model.previewSkinWeights();
    const bool skinned = std::any_of(skin.begin(), skin.end(), [](const auto& vertex) {
        return vertex.valid && vertex.influenceCount > 0;
    });
    const bool nativeRigidCutscene = !animation.isIfp() &&
        model.modelKind() == StorylandModelKind::CutsceneModel &&
        storylandNativeCutsceneHasRigidGeometry(model);
    if ((animation.isIfp() || staticBindPose || nativeRigidCutscene) && !model.isMobileLcsDff() && !skinned)
        return storylandSampleNativeCutsceneFrames(model, animation, seconds, sourcePoints,
            animatedPoints, animatedBones, errorMessage,frames,staticBindPose,replaceRigidClumpRoot);
    std::vector<std::array<float,16>> matrices;
    if(!storylandSampleModelAnimationImpl(model,animation,seconds,false,sourcePoints,
        animatedPoints,animatedBones,errorMessage,nullptr,0,0,nullptr,0,0,frames?&matrices:nullptr,false,true,
        false,nullptr,0,nullptr,nullptr,staticBindPose))return false;
    if(frames)for(size_t index=0;index<matrices.size();++index) {
        const auto& bone=model.armatureBones().at(index);
        StorylandCutsceneFramePose pose;
        pose.offset=bone.offset;pose.boneId=bone.boneId;pose.name=bone.name;pose.matrix=matrices[index];
        frames->push_back(std::move(pose));
    }
    return true;
}


bool storylandSampleModelAnimationRelativeRoot(const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation,float firstSeconds,
    const StorylandAnimFile& secondAnimation,float secondSeconds,float secondWeight,
    const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,
    std::vector<std::array<float,16>>& animatedFrames,std::string& errorMessage,bool retainFirstRootOffset,
    StorylandAnimationTransition* transition) {
    if(!std::isfinite(secondWeight)||secondWeight<0||secondWeight>1) {
        errorMessage="Crouch association blend is outside 0..1.";return false;
    }
    return storylandSampleModelAnimationImpl(model,firstAnimation,firstSeconds,false,sourcePoints,
        animatedPoints,animatedBones,errorMessage,&secondAnimation,secondSeconds,secondWeight,
        nullptr,0,0,&animatedFrames,false,false,false,nullptr,retainFirstRootOffset?2u:1u,nullptr,transition);
}
