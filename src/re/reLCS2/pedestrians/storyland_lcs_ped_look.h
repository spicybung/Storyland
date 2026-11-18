#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_weapon_attachment.h"

inline bool storylandApplyLcsScriptedLook(StorylandMissionEnvironment& world,int32_t handle,
    const StorylandModelFile& model,const std::array<float,16>& modelWorld,
    const StorylandNativePoint& actorPosition,StorylandAnimationBlendState& blend,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry=true,
    bool drivingOrDragged=false) {
    const auto found=world.scriptedPedLooks.find(handle);
    if(world.resolvingScriptedLookTarget||found==world.scriptedPedLooks.end()||
        (!found->second.active&&!found->second.restoring))return true;
    auto& task=found->second;
    const auto character=world.missionCharacters.find(handle);
    const bool player=handle==1;
    if((player&&world.health<=0)||(!player&&(character==world.missionCharacters.end()||
        !character->second.alive||character->second.health<=0))) {
        world.scriptedPedLooks.erase(found);return true;
    }
    StorylandPedLookPose look;
    look.state=&task.pose;look.modelWorld=modelWorld;
    look.actorPosition={actorPosition.x,actorPosition.y,actorPosition.z};
    look.milliseconds=world.gameMilliseconds;
    look.modelForwardAngle=storylandPedModelForwardAngle(model);
    look.looking=task.active;
    const int32_t animationGroup=player?0:character->second.animationGroup;
    const auto wait=world.lcsWaitAnimations.find(handle);
    const bool ducking=(player?world.playerCrouch.active:character->second.crouch.active)||
        (wait!=world.lcsWaitAnimations.end()&&wait->second.state==14);
    look.allowTorso=!drivingOrDragged&&!ducking&&animationGroup!=45&&animationGroup!=48;
    if(task.active) {
        if(task.target==handle) {
            StorylandNativePoint component;
            if(!storylandPoseBoneWorldPoint(model,blend.boneFrames,3,modelWorld,component)) {
                error="Native looking cannot resolve the actor's torso component.";return false;
            }
            look.targetPosition={component.x,component.y,component.z};
        } else {
            if(!world.characterBoneMatrix) {
                error="Native looking requires the live target skeleton.";return false;
            }
            struct TargetSampling {
                bool& active;
                explicit TargetSampling(bool& value):active(value) {active=true;}
                ~TargetSampling() {active=false;}
            } sampling(world.resolvingScriptedLookTarget);
            std::array<float,16> target;
            if(!world.characterBoneMatrix(task.target,3,target)) {
                error="Native looking cannot sample the target's authored torso component.";return false;
            }
            look.targetPosition={target[12],target[13],target[14]};
        }
    }
    if(!storylandApplyNativePedLook(model,look,blend.boneFrames,points,error,skinGeometry))return false;
    if(task.restoring&&task.pose.restored)task.restoring=false;
    return true;
}

inline bool storylandApplyLcsStandingLook(StorylandMissionEnvironment& world,int32_t handle,
    const StorylandModelFile& model,const StorylandNativePoint& position,float heading,
    StorylandAnimationBlendState& blend,std::vector<StorylandModelPoint>& points,
    std::string& error,bool skinGeometry=true,float height=1.8f) {
    if(world.resolvingScriptedLookTarget||!world.scriptedPedLooks.count(handle))return true;
    std::array<float,16> frame;
    if(!storylandStandingPedWorldFrame(model,position,heading,height,frame)) {
        error="Native looking cannot resolve the actor's model transform.";return false;
    }
    return storylandApplyLcsScriptedLook(world,handle,model,frame,position,blend,
        points,error,skinGeometry);
}

inline std::array<float,16> storylandLcsLookSeatFrame(const StorylandModelFile& model,
    const std::array<float,16>& seat,const std::array<float,3>& anchor={}) {
    const float angle=(90-storylandPedModelForwardAngle(model))*.0174532925199433f;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    auto frame=seat;
    for(size_t row=0;row<3;++row) {
        frame[row]=seat[row]*cosine+seat[4+row]*sine;
        frame[4+row]=-seat[row]*sine+seat[4+row]*cosine;
        frame[12+row]+=seat[row]*anchor[0]+seat[4+row]*anchor[1]+seat[8+row]*anchor[2];
    }
    return frame;
}
