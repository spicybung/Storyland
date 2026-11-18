#pragma once
#include "src/leeds/leeds_vehicle.h"
#include "src/leeds/leeds_model.h"
#include <array>
#include <cmath>
#include <limits>

using StorylandVehicleMatrix=std::array<float,16>;
inline StorylandVehicleMatrix storylandVehicleMatrixProduct(const StorylandVehicleMatrix& a,const StorylandVehicleMatrix& b) {
    StorylandVehicleMatrix result{};
    for(size_t column=0;column<4;++column)for(size_t row=0;row<4;++row)
        for(size_t k=0;k<4;++k)result[column*4+row]+=a[k*4+row]*b[column*4+k];
    return result;
}
inline StorylandVehicleMatrix storylandVehicleMeshFrame(const StorylandVehicleDefinition& definition,
    const std::array<float,3>& position,float heading,float minimumZ,float suspension,
    float roll,float pitch,bool water=false,const StorylandVehicleMatrix* nativeFrame=nullptr) {
    const auto inversePreviewRoot=[&]() {
        StorylandVehicleMatrix inverse{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        for(size_t column=0;column<3;++column)for(size_t row=0;row<3;++row)
            inverse[column*4+row]=definition.previewRoot[row*4+column];
        for(size_t row=0;row<3;++row)
            inverse[12+row]=-(inverse[row]*definition.previewRoot[12]+
                inverse[4+row]*definition.previewRoot[13]+inverse[8+row]*definition.previewRoot[14]);
        return inverse;
    };
    if(nativeFrame)return storylandVehicleMatrixProduct(*nativeFrame,inversePreviewRoot());
    constexpr float rad=.01745329251994329577f;
    const float yaw=(heading-90)*rad,c=std::cos(yaw),s=std::sin(yaw);
    StorylandVehicleMatrix frame{c,s,0,0,-s,c,0,0,0,0,1,0,position[0],position[1],position[2],1};
    const bool native=definition.hasNativeSuspension&&!water;
    if(!water || roll != 0 || pitch != 0) {
        const float cr=std::cos(roll*rad),sr=std::sin(roll*rad),cp=std::cos(pitch*rad),sp=std::sin(pitch*rad);
        StorylandVehicleMatrix lift{1,0,0,0,0,1,0,0,0,0,1,0,0,0,
            water?0:(native?suspension:std::clamp(suspension,-.15f,.15f))+(native?definition.nativeRideHeight:0),1};
        const StorylandVehicleMatrix ry{cr,0,-sr,0,0,1,0,0,sr,0,cr,0,0,0,0,1};
        const StorylandVehicleMatrix rx{1,0,0,0,0,cp,sp,0,0,-sp,cp,0,0,0,0,1};
        // CMatrix::SetRotate 0x47AAC8 keeps forward.z=sin(pitch), so roll
        // follows pitch in the local matrix (Rz * Rx * Ry).
        frame=storylandVehicleMatrixProduct(storylandVehicleMatrixProduct(storylandVehicleMatrixProduct(frame,lift),rx),ry);
    }
    if(water&&definition.vcsModelLayout&&definition.domain==StorylandVehicleDomain::Water&&
        (definition.nativeType==1||definition.nativeType==2)) {
        // Native CreateRwObject3C2AD0 replaces the authored root with the entity matrix.
        return storylandVehicleMatrixProduct(frame,inversePreviewRoot());
    }
    StorylandVehicleMatrix ground{1,0,0,0,0,1,0,0,0,0,1,0,
        native?-definition.previewRoot[12]:0,native?-definition.previewRoot[13]:0,
        water?0:native?-definition.previewRoot[14]:-minimumZ,1};
    return storylandVehicleMatrixProduct(frame,ground);
}
inline StorylandVehicleMatrix storylandVehicleRenderFrame(const StorylandVehicleDefinition& definition,
    const std::array<float,3>& position,float heading,float minimumZ,float suspension,
    float roll,float pitch,bool water=false,const StorylandVehicleMatrix* nativeFrame=nullptr,
    float bikeVisualLean=0) {
    const auto physical=storylandVehicleMeshFrame(definition,position,heading,minimumZ,suspension,
        roll,pitch,water,nativeFrame);
    if(!definition.twoWheels()||!definition.hasNativeSuspension||
        !std::isfinite(bikeVisualLean)||bikeVisualLean==0)return physical;
    const auto& lines=definition.physicalWheels();
    if(lines.empty())return physical;
    float minimum=lines.front().lowerZ;
    if(definition.collision)minimum=std::min(minimum,definition.collision->minimum.z);
    if(!std::isfinite(minimum))return physical;
    const float leanCosine=std::cos(bikeVisualLean),leanSine=std::sin(bikeVisualLean);
    const float pitchAngle=-.05f*std::fabs(bikeVisualLean);
    const float pitchCosine=std::cos(pitchAngle),pitchSine=std::sin(pitchAngle);
    // Both retail CalculateLeanMatrix routines use entity * Ry(lean) * Rx(tilt).
    const StorylandVehicleMatrix local{leanCosine,0,-leanSine,0,
        leanSine*pitchSine,pitchCosine,leanCosine*pitchSine,0,
        leanSine*pitchCosine,-pitchSine,leanCosine*pitchCosine,0,0,0,0,1};
    const auto entity=storylandVehicleMatrixProduct(physical,definition.previewRoot);
    auto leaned=storylandVehicleMatrixProduct(entity,local);
    const float lowering=(1-leanCosine)*minimum;
    for(size_t axis=0;axis<3;++axis)leaned[12+axis]+=entity[8+axis]*lowering;
    return storylandVehicleMeshFrame(definition,position,heading,minimumZ,suspension,
        roll,pitch,water,&leaned);
}
inline StorylandVehicleMatrix storylandVehicleOccupantFrame(const StorylandVehicleDefinition& definition,
    const std::array<float,3>& position,float heading,float minimumZ,float suspension,
    float roll,float pitch,unsigned seat=0,bool water=false,const StorylandVehicleMatrix* nativeFrame=nullptr,
    float nativeAngularZ=0,float bikeVisualLean=0) {
    auto frame=storylandVehicleMatrixProduct(storylandVehicleRenderFrame(definition,position,heading,minimumZ,
        suspension,roll,pitch,water,nativeFrame,bikeVisualLean),definition.previewRoot);
    if(definition.vcsJetSki()) {
        StorylandVehicleMatrix component{};
        if(!storylandVcsJetSkiChassisFrame(definition.vcsJetSkiChassis,nativeAngularZ,component)) {
            frame.fill(std::numeric_limits<float>::quiet_NaN());return frame;
        }
        frame=storylandVehicleMatrixProduct(frame,component);
    }
    auto seatPoint=definition.seatPosition(seat);
    if(definition.vcsJetSki()&&seat>=2&&seat<=3) {
        seatPoint=definition.dummyPositions[5];
        if(seat==2)seatPoint[0]=-seatPoint[0];
    }
    const StorylandVehicleMatrix local{1,0,0,0,0,1,0,0,0,0,1,0,seatPoint[0],seatPoint[1],seatPoint[2],1};
    return storylandVehicleMatrixProduct(frame,local);
}
struct StorylandVehiclePose {
    std::array<float,3> position{};
    float heading=0,roll=0,pitch=0;
};
// Crane swing preserves the native, slightly scaled frame while carrying.
// The free vehicle solver uses a rigid pose, so remove that scale before
// decomposing its Rz * Rx * Ry basis on release.
inline bool storylandVehiclePoseFromNativeFrame(const StorylandVehicleDefinition& definition,
    const StorylandVehicleMatrix& frame,float suspension,StorylandVehiclePose& result) {
    if(!std::isfinite(suspension)||!std::all_of(frame.begin(),frame.end(),
        [](float value){return std::isfinite(value);})||
        frame[3]!=0||frame[7]!=0||frame[11]!=0||frame[15]!=1)return false;
    const auto rigidBasis=[](const StorylandVehicleMatrix& matrix) {
        const auto dot=[&](size_t a,size_t b) {
            return matrix[a]*matrix[b]+matrix[a+1]*matrix[b+1]+matrix[a+2]*matrix[b+2];
        };
        for(size_t column:{size_t(0),size_t(4),size_t(8)})
            if(std::fabs(dot(column,column)-1)>.001f)return false;
        if(std::fabs(dot(0,4))>.001f||std::fabs(dot(0,8))>.001f||std::fabs(dot(4,8))>.001f)return false;
        const float determinant=matrix[0]*(matrix[5]*matrix[10]-matrix[6]*matrix[9])+
            matrix[1]*(matrix[6]*matrix[8]-matrix[4]*matrix[10])+
            matrix[2]*(matrix[4]*matrix[9]-matrix[5]*matrix[8]);
        return determinant>0;
    };
    auto rigid=frame;
    for(size_t column=0;column<3;++column) {
        const size_t at=column*4;
        const float length=std::hypot(rigid[at],rigid[at+1],rigid[at+2]);
        if(!std::isfinite(length)||length<.000001f)return false;
        for(size_t row=0;row<3;++row)rigid[at+row]/=length;
    }
    if(!rigidBasis(rigid))return false;
    const auto mesh=storylandVehicleMeshFrame(definition,{},0,0,0,0,0,false,&rigid);
    if(!std::all_of(mesh.begin(),mesh.end(),[](float value){return std::isfinite(value);})||
        !rigidBasis(mesh))return false;
    constexpr float degrees=57.29577951308232f;
    StorylandVehiclePose pose;
    const float cosinePitch=std::hypot(mesh[4],mesh[5]);
    pose.pitch=std::atan2(mesh[6],cosinePitch)*degrees;
    if(cosinePitch<.000001f) {
        pose.heading=std::atan2(mesh[1],mesh[0])*degrees+90;pose.roll=0;
    } else {
        pose.heading=std::atan2(-mesh[4],mesh[5])*degrees+90;
        pose.roll=std::atan2(-mesh[2],mesh[10])*degrees;
    }
    if(definition.hasNativeSuspension)
        pose.position={frame[12],frame[13],frame[14]-definition.nativeRideHeight-suspension};
    else pose.position={mesh[12]+mesh[8]*definition.previewMinimumZ,
        mesh[13]+mesh[9]*definition.previewMinimumZ,
        mesh[14]+mesh[10]*definition.previewMinimumZ-std::clamp(suspension,-.15f,.15f)};
    if(!std::all_of(pose.position.begin(),pose.position.end(),
        [](float value){return std::isfinite(value);}))return false;
    result=pose;return true;
}
inline bool storylandVehicleBikeRenderPose(const StorylandVehicleDefinition& definition,
    const std::array<float,3>& position,float heading,float suspension,float roll,float pitch,
    float bikeVisualLean,StorylandVehiclePose& result,const StorylandVehicleMatrix* nativeFrame=nullptr) {
    if(!definition.twoWheels())return false;
    const auto rendered=storylandVehicleMatrixProduct(storylandVehicleRenderFrame(definition,
        position,heading,definition.previewMinimumZ,suspension,roll,pitch,false,nativeFrame,bikeVisualLean),
        definition.previewRoot);
    return storylandVehiclePoseFromNativeFrame(definition,rendered,suspension,result);
}
inline std::array<float,3> storylandVehicleTransformPoint(const StorylandVehicleMatrix& frame,
    const std::array<float,3>& point) {
    return {frame[0]*point[0]+frame[4]*point[1]+frame[8]*point[2]+frame[12],
        frame[1]*point[0]+frame[5]*point[1]+frame[9]*point[2]+frame[13],
        frame[2]*point[0]+frame[6]*point[1]+frame[10]*point[2]+frame[14]};
}
// Vehicle frames are rigid transforms. Invert their orthonormal basis without
// dropping the model-root translation used by the decoded vehicle mesh.
inline std::array<float,3> storylandVehicleInversePoint(const StorylandVehicleMatrix& frame,
    const std::array<float,3>& point) {
    const float x=point[0]-frame[12],y=point[1]-frame[13],z=point[2]-frame[14];
    return {frame[0]*x+frame[1]*y+frame[2]*z,frame[4]*x+frame[5]*y+frame[6]*z,
        frame[8]*x+frame[9]*y+frame[10]*z};
}
// Endpoint correction retains the authored trajectory between the two anchors.
inline std::array<float,3> storylandVehicleWarpMotion(const StorylandModelPoint& start,
    const StorylandModelPoint& current,const StorylandModelPoint& end,
    const std::array<float,3>& from,const std::array<float,3>& to,float fraction,
    float modelRotationDegrees=0.0f) {
    const float t=std::clamp(fraction,0.0f,1.0f);
    const float radians=modelRotationDegrees*.01745329251994329577f;
    const float cosine=std::cos(radians),sine=std::sin(radians);
    const float dx=current.x-start.x-(end.x-start.x)*t;
    const float dy=current.y-start.y-(end.y-start.y)*t;
    const float dz=current.z-start.z-(end.z-start.z)*t;
    // Decoded animation displacement is in the ped model's local axes. The
    // occupant renderer rotates that model inside the vehicle's seat frame;
    // rotate the residual by the same amount before applying it to the anchor.
    return {from[0]+(to[0]-from[0])*t+cosine*dx-sine*dy,
        from[1]+(to[1]-from[1])*t+sine*dx+cosine*dy,
        from[2]+(to[2]-from[2])*t+dz};
}
