#pragma once
#include "src/leeds/leeds_model.h"
#include "src/leeds/leeds_anim.h"
#include "src/storyland/storyland_ped_look.h"
#include <array>

struct StorylandAnimationTransition {
    uint64_t key=0,appliedKey=0;
    float seconds=0,lastSeconds=0,blendRate=8,weight=1;
    bool initialized=false;
    std::vector<std::array<float,16>> previousLocal,sourceLocal;
};

bool storylandBlendAnimationFrames(const std::vector<uint32_t>& parents,
    std::vector<std::array<float,16>>& frames,StorylandAnimationTransition& transition,std::string& error);

bool storylandNativeAnimationForwardAngle(const StorylandModelFile& model,float& degrees);
std::array<float,2> storylandNativeAnimationRootDelta(const StorylandAnimFile& animation,
    uint16_t associationFlags,float fromSeconds,float toSeconds,bool mirrored);

struct StorylandAnimationPoseLayer {
    const StorylandAnimFile* animation=nullptr;
    float seconds=0;
    float weight=1;
    bool partial=false;
    const std::vector<StorylandAnimTrack>* tracks=nullptr;
    bool mirrored=false;
    bool upperBodyOnly=false;
};

enum class StorylandNativeRootPose { Script, LcsPedestrian, VcsPedestrian };

bool storylandSampleVcsScriptAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,
    const std::vector<uint16_t>& nativeFlags,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames=nullptr,
    StorylandAnimationTransition* transition=nullptr);

bool storylandSampleNativeRootAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,
    const std::vector<uint16_t>& nativeFlags,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames=nullptr,
    StorylandAnimationTransition* transition=nullptr,
    StorylandNativeRootPose pose=StorylandNativeRootPose::Script);

struct StorylandNativePedBoneFrame {
    uint32_t tag=0;
    std::array<float,16> matrix{};
};

bool storylandSampleVcsPedEntityPose(const StorylandModelFile& model,
    const StorylandAnimFile& animation,float seconds,
    std::vector<StorylandNativePedBoneFrame>& frames,std::string& error);

struct StorylandPedAimState {
    float torsoYaw=0,torsoPitch=0,upperYaw=0,upperPitch=0,lowerYaw=0,lowerPitch=0;
    float stepFraction=0,upperRoll=45,handRoll=30;
    bool lowerActive=false;
    bool initialized=false;
};
struct StorylandPedAimPose {
    StorylandPedAimState* state=nullptr;
    float yaw=0,pitch=0,seconds=0,weight=0,modelForwardAngle=90;
    bool aiming=false,arm=false;
    StorylandAnimationTransition* transition=nullptr;
};

bool storylandApplyNativePedLook(const StorylandModelFile& model,const StorylandPedLookPose& look,
    std::vector<std::array<float,16>>& frames,std::vector<StorylandModelPoint>& points,
    std::string& error,bool skinGeometry=true);

bool storylandSampleLcsAnimationAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames=nullptr);

bool storylandSampleNativeWeaponAssociations(const StorylandModelFile& model,
    const std::vector<StorylandAnimationPoseLayer>& associations,const StorylandPedAimPose& aim,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames=nullptr);

bool storylandSampleModelAnimationRelativeRoot(
    const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation,float firstSeconds,
    const StorylandAnimFile& secondAnimation,float secondSeconds,float secondWeight,
    const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones,
    std::vector<std::array<float,16>>& animatedFrames,std::string& errorMessage,
    bool retainFirstRootOffset=false,StorylandAnimationTransition* transition=nullptr);

bool storylandSampleMobileAnimation(
    const StorylandModelFile& model,
    const StorylandAnimFile& animation,
    float seconds,
    bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage);

bool storylandSampleModelAnimation(
    const StorylandModelFile& model,
    const StorylandAnimFile& animation,
    float seconds,
    bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage);

bool storylandSampleModelAnimationBlended(
    const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation, float firstSeconds,
    const StorylandAnimFile& secondAnimation, float secondSeconds, float secondWeight,
    bool lockRootMotion, const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones, std::string& errorMessage);

bool storylandSampleModelAnimationLayered(
    const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation, float firstSeconds,
    const StorylandAnimFile& secondAnimation, float secondSeconds, float secondWeight,
    const StorylandAnimFile* upperAnimation, float upperSeconds, float upperWeight,
    bool lockRootMotion, const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones, std::string& errorMessage);

bool storylandSampleModelAnimationLayered(
    const StorylandModelFile& model,
    const StorylandAnimFile& firstAnimation, float firstSeconds,
    const StorylandAnimFile& secondAnimation, float secondSeconds, float secondWeight,
    const StorylandAnimFile* upperAnimation, float upperSeconds, float upperWeight,
    bool lockRootMotion, const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones, std::string& errorMessage,
    std::vector<std::array<float,16>>* animatedFrames,
    StorylandAnimationTransition* transition=nullptr,
    bool preserveRootVertical=false);

bool storylandSampleModelAnimationLayeredAim(
    const StorylandModelFile& model,const StorylandAnimFile& firstAnimation,float firstSeconds,
    const StorylandAnimFile& secondAnimation,float secondSeconds,float secondWeight,
    const StorylandAnimFile* upperAnimation,float upperSeconds,float upperWeight,bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage,
    std::vector<std::array<float,16>>* animatedFrames,StorylandAnimationTransition* transition,
    bool preserveRootVertical,const StorylandPedAimPose* aim);

bool storylandSampleRetargetedCrouchTransition(const StorylandModelFile& model,
    const StorylandAnimFile& standing,const StorylandAnimFile& crouch,float seconds,
    float weight,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::string& error,
    std::vector<std::array<float,16>>* frames=nullptr);

bool storylandSampleRetargetedCrouchTransitionAim(const StorylandModelFile& model,
    const StorylandAnimFile& standing,const StorylandAnimFile& crouch,float seconds,
    float weight,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::string& error,
    std::vector<std::array<float,16>>* frames,const StorylandAnimFile* upperAnimation,
    float upperSeconds,float upperWeight,const StorylandPedAimPose* aim);

bool storylandPlantRetargetedCrouchFeet(const StorylandModelFile& model,
    std::vector<StorylandModelPoint>& points,
    std::vector<std::array<float,16>>* frames,std::string& error);

bool storylandSampleMirroredModelAnimation(const StorylandModelFile& model,
    const StorylandAnimFile& animation,float seconds,bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames=nullptr,
    StorylandAnimationTransition* transition=nullptr);

// Native ASSOC_PARTIAL overrides the bones for which the clip has tracks.
// It does not impose the upper-body mask used by the weapon aiming layer.
bool storylandSampleModelAnimationPartial(const StorylandModelFile& model,
    const StorylandAnimFile& baseAnimation,float baseSeconds,
    const StorylandAnimFile& partialAnimation,float partialSeconds,bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelPoint>& bones,std::string& error,
    std::vector<std::array<float,16>>* frames=nullptr);

struct StorylandCutsceneFramePose {
    uint32_t offset=0,boneId=UINT32_MAX;
    std::string name;
    std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
};

bool storylandSampleCutsceneModelAnimation(const StorylandModelFile& model,const StorylandAnimFile& animation,
    float seconds,const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,std::vector<StorylandModelPoint>& animatedBones,std::string& errorMessage,
    std::vector<StorylandCutsceneFramePose>* frames=nullptr,bool staticBindPose=false,
    bool replaceRigidClumpRoot=false);
