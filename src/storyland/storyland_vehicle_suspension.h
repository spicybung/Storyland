#pragma once
#include "src/leeds/leeds_vehicle.h"
#include "src/leeds/leeds_model.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_vehicle_wheels.h"

inline void storylandSetupVehicleExplosionOrigins(StorylandVehicleDefinition& definition,
    const StorylandModelFile& model) {
    definition.hasExplosionBurstOrigin=false;
    definition.hasExplosionJetOrigin=false;
    definition.explosionBurstOrigin={};
    definition.explosionJetOrigin={};
    if(definition.nativeType==1||(definition.vcsModelLayout&&definition.nativeType==2))return;
    const auto root=model.vehiclePreviewRootMatrix();
    const auto component=[&](uint32_t id,std::array<float,3>& result) {
        StorylandVehicleMatrix local{},parent{};
        std::string name;
        if(!model.vehicleComponentFrame(id,local,name,&parent,nullptr,definition.vcsModelLayout))return false;
        const auto preview=storylandVehicleMatrixProduct(parent,local);
        result=storylandVehicleInversePoint(root,{preview[12],preview[13],preview[14]});
        return std::all_of(result.begin(),result.end(),[](float value){return std::isfinite(value);});
    };
    if(definition.motorcycle()) {
        definition.hasExplosionBurstOrigin=component(3,definition.explosionBurstOrigin);
    } else if(!definition.bicycle()) {
        std::array<float,3> bumper{},wheel{};
        if(component(definition.vcsModelLayout?13u:8u,bumper)&&
            component(definition.vcsModelLayout?7u:6u,wheel)) {
            for(size_t axis=0;axis<3;++axis)
                definition.explosionBurstOrigin[axis]=(bumper[axis]+wheel[axis])*.5f;
            definition.hasExplosionBurstOrigin=true;
        } else definition.hasExplosionBurstOrigin=component(definition.vcsModelLayout?19u:18u,
            definition.explosionBurstOrigin);
    }
    definition.hasExplosionJetOrigin=component(definition.twoWheels()?3u:
        definition.vcsModelLayout?17u:14u,definition.explosionJetOrigin);
}

inline bool storylandSetupVehicleSuspension(StorylandVehicleDefinition& definition,const StorylandModelFile& model) {
    storylandSetupVehicleExplosionOrigins(definition,model);
    definition.hasSirenForkFrame=false;
    if(definition.vcsModelLayout&&definition.modelId==191) {
        StorylandVehicleMatrix local{},parent{};
        std::string name;
        if(model.vehicleComponentFrame(2,local,name,&parent,nullptr,true)) {
            const auto root=model.vehiclePreviewRootMatrix();
            StorylandVehicleMatrix inverse{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            for(size_t column=0;column<3;++column)for(size_t row=0;row<3;++row)
                inverse[column*4+row]=root[row*4+column];
            for(size_t row=0;row<3;++row)
                inverse[12+row]=-(inverse[row]*root[12]+inverse[4+row]*root[13]+inverse[8+row]*root[14]);
            definition.sirenForkFrame=storylandVehicleMatrixProduct(inverse,
                storylandVehicleMatrixProduct(parent,local));
            definition.hasSirenForkFrame=true;
        }
    }
    definition.hasNativeForklift=false;
    definition.forkliftFramePosition={};
    definition.forkliftPart.clear();
    if(definition.vcsModelLayout&&definition.modelId==251) {
        std::array<float,16> forkFrame{};
        std::string forkName;
        if(model.vehicleComponentFrame(22,forkFrame,forkName)) {
            definition.hasNativeForklift=true;
            definition.forkliftFramePosition={forkFrame[12],forkFrame[13],forkFrame[14]};
            definition.forkliftPart=std::move(forkName);
        }
    }
    definition.hasNativeSuspension=false;definition.wheels.clear();definition.auxiliaryWheels.clear();
    definition.suspensionLines.clear();
    // CQuadBike delegates suspension setup to CAutomobile at 0x401CA4.
    const bool quad=definition.vcsModelLayout&&definition.nativeType==9;
    const bool bike=definition.twoWheels();
    const bool hovercraft=definition.vcsHovercraft();
    if((definition.nativeType!=0&&!quad&&!bike&&!hovercraft)||!definition.hasNativeHandling||!definition.hasNativeDummies)return false;
    if(model.previewPoints().empty())return false;
    definition.previewRoot=model.vehiclePreviewRootMatrix();
    definition.previewMinimumZ=definition.previewMaximumZ=model.previewPoints().front().z;
    for(const auto& point:model.previewPoints()) {
        definition.previewMinimumZ=std::min(definition.previewMinimumZ,point.z);
        definition.previewMaximumZ=std::max(definition.previewMaximumZ,point.z);
    }
    if(!std::isfinite(definition.suspensionForce)||definition.suspensionForce<=0||
        !std::isfinite(definition.suspensionUpper)||!std::isfinite(definition.suspensionLower)||
        definition.suspensionUpper<=definition.suspensionLower)return false;
    // These preview roots are export translations. Reject an unsupported basis
    // rather than applying the vertical suspension equations in a rotated frame.
    for(unsigned column=0;column<3;++column)for(unsigned row=0;row<3;++row)
        if(std::fabs(definition.previewRoot[column*4+row]-(column==row?1.0f:0.0f))>.0001f)return false;
    auto frames=model.vehicleWheelFrames(quad);
    if(frames.empty())for(const auto& triangle:model.previewTriangles()) {
        if(!triangle.vehicleWheel)continue;
        const auto& pivot=triangle.vehiclePivot;
        const bool exists=std::any_of(frames.begin(),frames.end(),
            [&pivot](const StorylandVehicleWheelFrame& frame) {
                return std::fabs(frame.pivot.x-pivot.x)+std::fabs(frame.pivot.y-pivot.y)+
                    std::fabs(frame.pivot.z-pivot.z)<.0001f;
            });
        if(!exists)frames.push_back({triangle.vehiclePart,pivot,triangle.vehicleFrontWheel});
    }
    std::vector<StorylandVehicleWheelFrame> middleFrames;
    frames.erase(std::remove_if(frames.begin(),frames.end(),[&](const StorylandVehicleWheelFrame& frame) {
        if(frame.name.find("wheel_lm")!=0&&frame.name.find("wheel_rm")!=0)return false;
        middleFrames.push_back(frame);return true;
    }),frames.end());
    if(!definition.vcsModelLayout&&frames.size()==3u) {
        // Several LCS PS2 clumps omit one mirrored wheel dummy from the
        // serialized node traversal (landstal.mdl omits wheel_rf_dummy).
        // Restore the fourth pivot only when its axle has one wheel and
        // the other axle has a verified pair.
        const size_t frontCount=std::count_if(frames.begin(),frames.end(),
            [](const StorylandVehicleWheelFrame& frame){return frame.front;});
        if(frontCount==1u||frontCount==2u) {
            const bool missingFront=frontCount==1u;
            const auto counterpart=std::find_if(frames.begin(),frames.end(),
                [missingFront](const StorylandVehicleWheelFrame& frame) {
                    return frame.front==missingFront&&std::fabs(frame.pivot.x)>0.1f;
                });
            if(counterpart!=frames.end()) {
                StorylandVehicleWheelFrame mirrored=*counterpart;
                mirrored.pivot.x=-mirrored.pivot.x;
                mirrored.matrix[12]=mirrored.pivot.x;
                mirrored.matrix[13]=mirrored.pivot.y;
                mirrored.matrix[14]=mirrored.pivot.z;
                mirrored.name+="_mirrored";
                const bool unique=std::none_of(frames.begin(),frames.end(),
                    [&mirrored](const StorylandVehicleWheelFrame& frame) {
                        return frame.front==mirrored.front&&
                            std::fabs(frame.pivot.x-mirrored.pivot.x)<0.01f&&
                            std::fabs(frame.pivot.y-mirrored.pivot.y)<0.01f;
                    });
                if(unique)frames.push_back(std::move(mirrored));
            }
        }
    }
    if(!definition.vcsModelLayout&&middleFrames.size()==1u&&std::fabs(middleFrames.front().pivot.x)>.1f) {
        auto mirrored=middleFrames.front();mirrored.pivot.x=-mirrored.pivot.x;
        mirrored.matrix[12]=mirrored.pivot.x;mirrored.name+="_mirrored";
        middleFrames.push_back(std::move(mirrored));
    }
    frames.insert(frames.end(),middleFrames.begin(),middleFrames.end());
    for(const auto& frame:frames) {
        const auto& pivot=frame.pivot;
        bool exists=false;
        for(const auto& wheel:definition.wheels)
            if(std::fabs(wheel.previewPivot[0]-pivot.x)+std::fabs(wheel.previewPivot[1]-pivot.y)+
                std::fabs(wheel.previewPivot[2]-pivot.z)<.0001f) {exists=true;break;}
        if(exists)continue;
        StorylandVehicleWheelDefinition wheel;
        wheel.previewPivot={pivot.x,pivot.y,pivot.z};wheel.front=frame.front;
        wheel.previewFrame=frame.matrix;
        for(size_t axis=0;axis<3;++axis)wheel.previewFrame[12+axis]=wheel.previewPivot[axis];
        wheel.nativeZ=pivot.z-definition.previewRoot[14];
        wheel.radius=(wheel.front?definition.wheelScale:definition.rearWheelScale)*.5f;
        wheel.upperZ=wheel.nativeZ+definition.suspensionUpper;
        wheel.lowerZ=wheel.nativeZ+definition.suspensionLower-wheel.radius;
        wheel.springLength=definition.suspensionUpper-definition.suspensionLower;
        wheel.lineLength=wheel.upperZ-wheel.lowerZ;
        if(!std::isfinite(wheel.radius)||wheel.radius<=0||!std::isfinite(wheel.lineLength)||wheel.lineLength<=0) {
            definition.wheels.clear();return false;
        }
        const bool middle=frame.name.find("wheel_lm")==0||frame.name.find("wheel_rm")==0;
        if(middle)definition.auxiliaryWheels.push_back(wheel);
        else definition.wheels.push_back(wheel);
    }
    std::sort(definition.wheels.begin(),definition.wheels.end(),[&](const auto& a,const auto& b) {
        if(bike)return a.front&&!b.front;
        const int ai=(a.previewPivot[0]>=definition.previewRoot[12]?2:0)+(a.front?0:1);
        const int bi=(b.previewPivot[0]>=definition.previewRoot[12]?2:0)+(b.front?0:1);
        return ai<bi;
    });
    unsigned fronts=0;const StorylandVehicleWheelDefinition* front=nullptr;
    for(const auto& wheel:definition.wheels)if(wheel.front) {++fronts;if(!front)front=&wheel;}
    if(definition.wheels.size()!=(bike?2u:4u)||fronts!=(bike?1u:2u)) {
        definition.wheels.clear();return false;
    }
    if(bike) {
        // CBike::SetupSuspensionLines: VCS 0x335B28..0x3360F8,
        // LCS 0x1551C8..0x155788. The four
        // physical lines use the front diameter even when the rear mesh differs.
        for(const auto& visual:definition.wheels)for(float direction:{1.0f,-1.0f}) {
            auto line=visual;
            line.previewPivot[1]+=direction*definition.wheelScale*.25f;
            line.radius=definition.wheelScale*.5f;
            line.lowerZ=line.nativeZ+definition.suspensionLower-line.radius;
            line.lineLength=line.upperZ-line.lowerZ;
            definition.suspensionLines.push_back(line);
        }
    }
    if(definition.vcsModelLayout||bike) {
        // VCS 0x1347B4..0x134840 and LCS CBike 0x155638..0x155694.
        definition.nativeRideHeight=front->springLength*(1-1/(4*definition.suspensionForce))-
            front->upperZ+definition.wheelScale*.5f;
    } else {
        // LCS retains the GTA III/VC suspension-line equilibrium equation.
        // The tyre radius is included in the full line length.
        definition.nativeRideHeight=-front->upperZ+
            front->lineLength*(1-1/(8*definition.suspensionForce));
    }
    if(!std::isfinite(definition.nativeRideHeight)) {definition.wheels.clear();return false;}
    for(auto& wheel:definition.wheels)
        wheel.restZ=wheel.radius-definition.nativeRideHeight;
    for(auto& wheel:definition.suspensionLines)wheel.restZ=wheel.radius-definition.nativeRideHeight;
    for(auto& wheel:definition.auxiliaryWheels)wheel.restZ=wheel.radius-definition.nativeRideHeight;
    if(hovercraft&&definition.collision) {
        // The native automobile setup extends the COL bound after creating its
        // suspension lines (134844..1348D4). The primitive shapes are retained.
        auto collision=std::make_shared<StorylandNativeCollisionModel>(*definition.collision);
        collision->minimum.z=std::min(collision->minimum.z,definition.wheels.front().lowerZ);
        const auto length=[](const StorylandNativePoint& point) {
            return std::sqrt(point.x*point.x+point.y*point.y+point.z*point.z);
        };
        collision->boundingRadius=std::max(collision->boundingRadius,
            std::max(length(collision->minimum),length(collision->maximum)));
        definition.collision=std::move(collision);
    }
    storylandSetupVehicleTyreGeometry(definition,model);
    definition.hasNativeSuspension=true;return true;
}

inline float storylandVehicleSpringRatio(const StorylandVehicleWheelDefinition& wheel,float lineFraction) {
    if(!std::isfinite(lineFraction)||!std::isfinite(wheel.springLength)||
        !std::isfinite(wheel.lineLength)||wheel.springLength<=0||wheel.lineLength<=0)return 1.0f;
    const float springFraction=wheel.springLength/wheel.lineLength;
    return (lineFraction-(1-springFraction))/springFraction;
}

inline bool storylandNativeBikeTyreContactPoint(const StorylandVehicleDefinition& definition,
    const std::array<float,4>& springRatios,size_t wheel,std::array<float,3>& result) {
    if(!definition.twoWheels()||definition.suspensionLines.size()!=4||wheel>=2)return false;
    const size_t first=wheel*2;
    const auto& line=definition.suspensionLines[first];
    const float ratio=std::min(springRatios[first],springRatios[first+1]);
    if(!std::isfinite(springRatios[first])||!std::isfinite(springRatios[first+1])||
        !std::isfinite(line.springLength)||line.springLength<=0||
        !std::isfinite(definition.wheelScale)||definition.wheelScale<=0)return false;
    // LCS 15A640/15A6D8 and VCS 33A20C/33A2DC use the outer pair's Y:
    // front line 0, rear line 3. Both Z values use the axle's first line.
    const auto& longitudinal=definition.suspensionLines[wheel==0?0:3];
    const std::array<float,3> point{0,
        longitudinal.previewPivot[1]-definition.previewRoot[13],
        line.upperZ-ratio*line.springLength-definition.wheelScale*.5f};
    if(!std::all_of(point.begin(),point.end(),[](float value){return std::isfinite(value);}))return false;
    result=point;return true;
}
inline float storylandVehicleWheelTarget(const StorylandVehicleWheelDefinition& wheel,float springRatio) {
    return wheel.upperZ-std::clamp(springRatio,0.0f,1.0f)*wheel.springLength;
}
inline float storylandVehicleWheelFollow(float previous,float target,float seconds) {
    // CAutomobile wheel interpolation: VCS 0x13E598.
    if(!std::isfinite(seconds)||seconds<=0)return previous;
    return previous+(target-previous)*(1-std::pow(.25f,seconds*50));
}

inline float storylandNativeSpringImpulse(float mass,float force,float ratio,float bias,float seconds) {
    if(!std::isfinite(mass)||mass<=0||!std::isfinite(seconds)||seconds<=0||
        !std::isfinite(ratio)||!std::isfinite(force)||!std::isfinite(bias))return 0;
    // CPhysical::ApplySpringCollisionAlt, VCS 0x34E1D8.
    const float step=std::min(seconds*50.0f,3.0f);
    return .008f*mass*2.0f*force*std::max(0.0f,1-ratio)*step*bias*50.0f;
}
inline float storylandNativeSpringDamping(float damping,float limit,float speedBefore,
    float speedAfter,float effectiveMass,float seconds) {
    if(!std::isfinite(effectiveMass)||effectiveMass<=0||!std::isfinite(seconds)||seconds<=0||
        !std::isfinite(damping)||!std::isfinite(limit)||!std::isfinite(speedBefore)||
        !std::isfinite(speedAfter))return 0;
    // CPhysical::ApplySpringDampening, VCS 0x34E348.
    float correction=-speedBefore*std::clamp(damping*std::min(seconds*50.0f,3.0f),-.25f,.25f);
    if(correction>0&&correction+speedAfter>0)correction=speedAfter<0?-speedAfter:0;
    else if(correction<0&&correction+speedAfter<0)correction=speedAfter>0?-speedAfter:0;
    return std::min(correction*effectiveMass,std::fabs(limit)*.999f);
}

inline float storylandNativeVehicleAngularResistance(float drag,float seconds) {
    if(!std::isfinite(drag)||drag<0||!std::isfinite(seconds)||seconds<=0)return 1.0f;
    const float resistance=drag>.01f?drag*.0005f:drag;
    // VCS 0x3486A4 and 0x348740 damp all three angular components.
    const float retention=resistance>.1f?resistance:.99f;
    return std::pow(std::clamp(retention,0.0f,1.0f),seconds*50.0f);
}

inline StorylandNativePoint storylandVehicleSpringDirection(StorylandNativePoint axis,
    StorylandNativePoint normal,bool bike) {
    // CBike selects the axial spring at LCS 0x159964 / VCS 0x339424.
    if(bike&&normal.z<=.35f)return {-axis.x,-axis.y,-axis.z};
    if(axis.x*normal.x+axis.y*normal.y+axis.z*normal.z>0)
        return {-normal.x,-normal.y,-normal.z};
    return normal;
}

inline StorylandNativePoint storylandVehicleDampingDirection(StorylandNativePoint axis,
    StorylandNativePoint normal,float springRatio) {
    // VCS automobile 0x13DA24 and bike 0x339884 select the road-normal damper.
    if(springRatio<1.0f&&normal.z>.35f)return {-normal.x,-normal.y,-normal.z};
    return axis;
}
