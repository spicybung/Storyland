#pragma once
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_vehicle_wheels.h"
#include "src/re/reVCS/runtime/storyland_vcs_retail_quirks.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

struct StorylandWheelGroundSupport {
    bool valid = false;
    unsigned contacts = 0;
    float targetPositionZ = 0.0f;
    float maxPositionZ = 0.0f;
    float minPositionZ = 0.0f;
    std::array<bool,4> wheelOnGround{};
    std::array<float,4> wheelGroundHeight{};
    std::array<float,4> wheelVerticalGap{};
};

// The actor's road height is the plane the wheels support, not the road
// under the middle of the chassis. The ray origins are transformed wheel
// dummies after suspension and native ride height; they remain on the
// correct side of a banked or pitched vehicle.
template<class GroundProbe>
inline StorylandWheelGroundSupport storylandMeasureWheelGround(
    const StorylandVehicleDefinition& definition,
    const std::array<float,3>& actorPosition,
    float yaw, float suspension, float roll, float pitch,
    const std::array<float,4>& wheelPositions,
    GroundProbe&& groundProbe,float frontRotation=0,float rearRotation=0,float steeringAngle=0,
    const std::array<uint8_t,4>& tyreStatus={},bool handbrake=false,float bikeVisualLean=0) {
    StorylandWheelGroundSupport result;
    if (!definition.hasNativeSuspension || definition.wheels.empty() ||
        definition.wheels.size()>wheelPositions.size()) return result;
    const auto frame=storylandVehicleRenderFrame(definition,actorPosition,yaw,
        definition.previewMinimumZ,suspension,roll,pitch,false,nullptr,bikeVisualLean);
    if (!std::isfinite(frame[10]) || frame[10]<0.35f) return result;
    float sum=0.0f;
    result.minPositionZ=std::numeric_limits<float>::infinity();
    result.maxPositionZ=-std::numeric_limits<float>::infinity();
    for(size_t i=0;i<definition.wheels.size();++i) {
        const auto& wheel=definition.wheels[i];
        if(!std::isfinite(wheel.radius)||wheel.radius<=0.0f||!std::isfinite(wheel.restZ))continue;
        auto local=wheel.previewPivot;
        local[2]+=wheelPositions[i]-wheel.nativeZ;
        const auto center=storylandVehicleTransformPoint(frame,local);
        if(!std::isfinite(center[0])||!std::isfinite(center[1])||!std::isfinite(center[2]))continue;
        float floor=0.0f;std::array<float,3> normal{};
        if(!groundProbe(center[0],center[1],center[2],wheel.radius,floor,normal)||!std::isfinite(floor))continue;
        const float normalLength=std::hypot(normal[0],normal[1],normal[2]);
        if(!std::isfinite(normalLength)||normalLength<.000001f)continue;
        for(float& axis:normal)axis/=normalLength;
        const float projection=normal[0]*frame[8]+normal[1]*frame[9]+normal[2]*frame[10];
        if(normal[2]<.45f||!std::isfinite(projection)||projection<.35f)continue;
        const bool left=wheel.previewPivot[0]<definition.previewRoot[12];
        const size_t tyre=storylandVehicleTyreIndex(definition.twoWheels(),wheel.front,left);
        const float steering=StorylandVcsRetailQuirks::wheelSteerDegrees(
            definition.vcsModelLayout,definition.handlingFlags,wheel.front,steeringAngle,handbrake);
        const float extent=storylandVehicleTyreExtent(wheel,frame,normal,
            wheel.front?frontRotation:rearRotation,steering,tyreStatus[tyre]!=0);
        if(!std::isfinite(extent)||extent<=0)continue;
        const float gap=center[2]-floor-extent/normal[2];
        // Outside the native tyre/suspension reach this is a fall or a wall,
        // not a valid road surface to teleport the body onto.
        if(gap<-0.85f || gap>1.65f)continue;
        // Keep the road anchor independent of dynamic body compression and
        // animated wheel travel. The actual tyre pose above still determines
        // whether the road is within reach. Including its deflection in the
        // anchor would let spring forces push the anchor below a supported body.
        const float dynamicHeight=suspension+(wheelPositions[i]-wheel.restZ)*projection/normal[2];
        const float target=actorPosition[2]-gap+dynamicHeight;
        result.wheelOnGround[i]=true;
        result.wheelGroundHeight[i]=floor;
        result.wheelVerticalGap[i]=gap;
        result.minPositionZ=std::min(result.minPositionZ,target);
        result.maxPositionZ=std::max(result.maxPositionZ,target);
        sum+=target;
        ++result.contacts;
    }
    if(result.contacts) {
        // Sloped roads use pitch and roll from the contact normals. Keep the
        // body supported by all available tyre contacts while allowing
        // different spring lengths on uneven terrain.
        const float mean=sum/float(result.contacts);
        result.targetPositionZ=mean+std::min(0.10f,result.maxPositionZ-mean);
        result.valid=true;
    }
    return result;
}

inline float storylandWheelContactCorrection(const StorylandVehicleWheelDefinition& wheel,
    float currentPosition, float groundGap, float axisZ, float radiusDifference=0.0f) {
    if(!std::isfinite(groundGap)||!std::isfinite(axisZ)||axisZ<0.35f||
        !std::isfinite(currentPosition)||!std::isfinite(wheel.radius))return currentPosition;
    if(!std::isfinite(wheel.springLength)||wheel.springLength<=0||
        !std::isfinite(wheel.upperZ)||!std::isfinite(radiusDifference))return currentPosition;
    const float upper=wheel.upperZ+radiusDifference;
    return std::clamp(currentPosition-groundGap/axisZ,upper-wheel.springLength,upper);
}

struct StorylandWheelPlaneSupport {
    bool valid=false;
    float gap=0,extent=0,suspensionProjection=0;
};

inline StorylandWheelPlaneSupport storylandMeasureWheelPlane(
    const StorylandVehicleWheelDefinition& wheel,float currentPosition,
    const StorylandVehicleMatrix& bodyFrame,const std::array<float,3>& contact,
    const std::array<float,3>& normal,float rollingAngle=0,float steeringAngle=0,bool burst=false) {
    StorylandWheelPlaneSupport result;
    const float length=std::hypot(normal[0],normal[1],normal[2]);
    if(!std::isfinite(length)||length<.000001f||!std::isfinite(currentPosition)||
        !std::isfinite(rollingAngle)||!std::isfinite(steeringAngle))return result;
    const std::array<float,3> unit{normal[0]/length,normal[1]/length,normal[2]/length};
    const float projection=unit[0]*bodyFrame[8]+unit[1]*bodyFrame[9]+unit[2]*bodyFrame[10];
    if(!std::isfinite(projection)||projection<.35f||unit[2]<.45f)return result;
    auto local=wheel.previewPivot;local[2]+=currentPosition-wheel.nativeZ;
    const auto center=storylandVehicleTransformPoint(bodyFrame,local);
    const float extent=storylandVehicleTyreExtent(wheel,bodyFrame,unit,rollingAngle,steeringAngle,burst);
    const float gap=(center[0]-contact[0])*unit[0]+(center[1]-contact[1])*unit[1]+
        (center[2]-contact[2])*unit[2]-extent;
    if(!std::isfinite(extent)||extent<=0||!std::isfinite(gap))return result;
    result.valid=true;result.gap=gap;result.extent=extent;result.suspensionProjection=projection;
    return result;
}

inline float storylandWheelPlaneCorrection(const StorylandVehicleWheelDefinition& wheel,
    float physicalRadius,float currentPosition,const StorylandVehicleMatrix& bodyFrame,
    const std::array<float,3>& contact,const std::array<float,3>& normal,
    float rollingAngle=0,float steeringAngle=0,bool burst=false,
    const StorylandVehicleMatrix* physicalFrame=nullptr,
    const StorylandVehicleWheelDefinition* physicalLine=nullptr) {
    if(!std::isfinite(physicalRadius)||physicalRadius<=0||
        !std::isfinite(wheel.springLength)||wheel.springLength<=0||!std::isfinite(wheel.upperZ))return currentPosition;
    const auto support=storylandMeasureWheelPlane(wheel,currentPosition,bodyFrame,contact,normal,
        rollingAngle,steeringAngle,burst);
    if(!support.valid||support.gap<-.5f||support.gap>1.0f)return currentPosition;
    const float radiusOffset=support.extent/support.suspensionProjection-physicalRadius;
    float upper=wheel.upperZ+radiusOffset,lower=upper-wheel.springLength;
    if(physicalFrame||physicalLine) {
        if(!physicalFrame||!physicalLine||!std::isfinite(physicalLine->springLength)||
            physicalLine->springLength<=0||!std::isfinite(physicalLine->upperZ)||
            !std::isfinite(physicalLine->nativeZ))return currentPosition;
        // Native lean and lowering move the rendered suspension axis. Map the
        // selected physical ray's support planes into that axis, preserving
        // its actual spring reach and longitudinal offset.
        auto upperLocal=physicalLine->previewPivot;
        upperLocal[2]+=physicalLine->upperZ-physicalLine->nativeZ-physicalRadius;
        const auto upperPoint=storylandVehicleTransformPoint(*physicalFrame,upperLocal);
        auto lowerLocal=upperLocal;
        lowerLocal[2]-=physicalLine->springLength;
        const auto lowerPoint=storylandVehicleTransformPoint(*physicalFrame,lowerLocal);
        const float length=std::hypot(normal[0],normal[1],normal[2]);
        const auto positionOnPlane=[&](const std::array<float,3>& point) {
            const float distance=((point[0]-contact[0])*normal[0]+(point[1]-contact[1])*normal[1]+
                (point[2]-contact[2])*normal[2])/length;
            return currentPosition+(distance-support.gap)/support.suspensionProjection;
        };
        upper=positionOnPlane(upperPoint);lower=positionOnPlane(lowerPoint);
        if(!std::isfinite(upper)||!std::isfinite(lower)||upper<lower)return currentPosition;
    }
    return std::clamp(currentPosition-support.gap/support.suspensionProjection,lower,upper);
}
