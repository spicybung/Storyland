#include "src/storyland/storyland_extended_movement.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_native_body.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float Radians = 0.01745329251994329577f;
constexpr float PedOriginAboveFeet = 1.0f;
constexpr float HangingHorizontal = -0.40f;
constexpr float HangingVertical = -1.10f;
constexpr float EdgeHorizontal = -0.40f;
constexpr float StandHorizontal = 0.15f;
constexpr float StandVertical = 1.0f;
constexpr float MinimumStretchGrab = 1.40f;

void changeAction(StorylandExtendedMovement& state, StorylandExtendedAction action) {
    state.action = action;
    state.elapsed = 0.0f;
}

StorylandNativePoint interpolation(const StorylandNativePoint& a,
    const StorylandNativePoint& b, float fraction) {
    return {a.x + (b.x - a.x) * fraction,
        a.y + (b.y - a.y) * fraction,
        a.z + (b.z - a.z) * fraction};
}

StorylandNativePoint ledgeOffset(const StorylandNativePoint& ledge,
    float heading, float horizontal, float vertical) {
    return {ledge.x + std::cos(heading * Radians) * horizontal,
        ledge.y + std::sin(heading * Radians) * horizontal, ledge.z + vertical};
}

bool sphereClear(const StorylandNativeWorld& world, const StorylandNativePoint& center,
    float radius) {
    bool hit = false;
    return storylandNativeWorldSphereContact(world, center, radius, 0x3FFFu, true, hit) && !hit;
}

bool climbPoseClear(const StorylandNativeWorld& world, const StorylandNativePoint& ledge,
    float heading, float floorHeight, bool vault) {
    struct Sphere { float forward, height, radius; };
    // SA's climb task checks its stand/vault volumes, not a standing ped box
    // through the lip while the authored pose is pulling the legs over it.
    constexpr std::array<Sphere,7> stand{{{.6f,1.75f,.35f},{.6f,1.30f,.35f},
        {.6f,.85f,.35f},{.3f,.65f,.35f},{0,.50f,.35f},
        {0,.10f,.35f},{0,-.30f,.35f}}};
    constexpr std::array<Sphere,6> over{{{1.10f,-.2f,.3f},{1.10f,.2f,.3f},
        {1.10f,.6f,.3f},{.70f,.6f,.3f},{.35f,.6f,.3f},{0,.6f,.3f}}};
    const auto clear = [&](const Sphere& sphere) {
        auto center = ledgeOffset(ledge,heading,EdgeHorizontal+sphere.forward,sphere.height);
        // Storyland stores feet rather than the SA ped origin. A grounded
        // approach must not count its own support floor as a blocked pose.
        if(sphere.forward == 0 && center.z-sphere.radius < floorHeight)
            center.z = floorHeight+sphere.radius+.001f;
        return sphereClear(world,center,sphere.radius);
    };
    if(vault) {
        for(const auto& sphere:over)if(!clear(sphere))return false;
    } else {
        for(const auto& sphere:stand)if(!clear(sphere))return false;
    }
    return true;
}

bool supportAt(const StorylandNativeWorld& world, const StorylandNativePoint& point,
    float above, float below, StorylandNativePoint& floor) {
    const StorylandNativePoint top{point.x,point.y,point.z+above};
    const StorylandNativePoint bottom{point.x,point.y,point.z-below};
    StorylandNativePoint normal{};uint8_t surface=0;
    const float hit=storylandTraceNativeGround(world,top,bottom,surface,&normal);
    if(hit>=1.0f || normal.z<.70f)return false;
    floor={point.x,point.y,top.z+(bottom.z-top.z)*hit};
    return true;
}

bool sideLipPresent(const StorylandNativeWorld& world,const StorylandNativePoint& ledge,float heading) {
    const auto outside=ledgeOffset(ledge,heading,-.18f,-.04f);
    const auto inside=ledgeOffset(ledge,heading,.18f,-.04f);
    return storylandTraceNativeSegment(world,outside,inside)<.99f;
}

StorylandNativeBody climbBody(float heading) {
    auto body=storylandNativeCharacterBody();
    body.yaw=heading;
    return body;
}

bool moveApproach(const StorylandNativeWorld& world, StorylandNativePoint& position,
    const StorylandNativePoint& next, float heading) {
    const auto moved=storylandMoveNativeBody(world,position,next,climbBody(heading));
    if(!moved.clear || moved.blocked || moved.recovered)return false;
    position=moved.position;
    return true;
}

void dropClimb(StorylandExtendedMovement& state, StorylandPlayTestRuntime& runtime,
    const StorylandNativeWorld& world, StorylandNativePoint& position) {
    auto outside=ledgeOffset(state.climbLedge,state.climbHeading,EdgeHorizontal,
        position.z-state.climbLedge.z);
    if(storylandNativeBodyClear(world,outside,climbBody(state.climbHeading)))position=outside;
    state.cancel();
    runtime.grounded=false;
    runtime.verticalSpeed=-.5f;
    runtime.movementSpeed=0;
    runtime.airborneTime=0;
}

float rootProgress(const StorylandPlayTestRuntime& runtime, StorylandExtendedAction action,
    float seconds, float fallback) {
    if(runtime.extendedClimbRootProgress) {
        const float sampled=runtime.extendedClimbRootProgress(action,seconds);
        if(std::isfinite(sampled))return std::clamp(sampled,0.0f,1.0f);
    }
    return fallback;
}
}

float storylandExtendedActionDuration(StorylandExtendedAction action) {
    switch (action) {
    case StorylandExtendedAction::ClimbJump: return 0.566667f;
    case StorylandExtendedAction::ClimbIdle: return 0.8f;
    case StorylandExtendedAction::ClimbPull: return 0.866667f;
    case StorylandExtendedAction::ClimbStand: return 0.8f;
    case StorylandExtendedAction::ClimbFinish: return 0.2f;
    case StorylandExtendedAction::ClimbVault: return 0.966667f;
    case StorylandExtendedAction::ClimbVaultFinish: return 0.0333333f;
    case StorylandExtendedAction::CrouchRollLeft:
    case StorylandExtendedAction::CrouchRollRight: return 0.933333f;
    case StorylandExtendedAction::None: return 0.0f;
    }
    return 0.0f;
}

const char* storylandExtendedActionClip(StorylandExtendedAction action) {
    switch (action) {
    case StorylandExtendedAction::ClimbJump: return "CLIMB_jump";
    case StorylandExtendedAction::ClimbIdle: return "CLIMB_idle";
    case StorylandExtendedAction::ClimbPull: return "CLIMB_Pull";
    case StorylandExtendedAction::ClimbStand: return "CLIMB_Stand";
    case StorylandExtendedAction::ClimbFinish: return "CLIMB_Stand_finish";
    case StorylandExtendedAction::ClimbVault: return "CLIMB_jump_B";
    case StorylandExtendedAction::ClimbVaultFinish: return "CLIMB_jump2fall";
    case StorylandExtendedAction::CrouchRollLeft: return "Crouch_Roll_L";
    case StorylandExtendedAction::CrouchRollRight: return "Crouch_Roll_R";
    case StorylandExtendedAction::None: return "";
    }
    return "";
}

bool storylandExtendedActionLoops(StorylandExtendedAction action) {
    return action==StorylandExtendedAction::ClimbIdle;
}

bool storylandTryStartClimb(StorylandExtendedMovement& state, const StorylandNativeWorld& world,
    const StorylandNativePoint& position, float headingDegrees, bool airborne) {
    if(state.busy() || state.crouched || !std::isfinite(headingDegrees) ||
        !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))return false;
    const float heading=headingDegrees*Radians;
    const float forwardX=std::cos(heading),forwardY=std::sin(heading);
    constexpr float maximumProbe=1.75f;
    float wallDistance=maximumProbe;
    bool wallFound=false;
    for(float height:{airborne?.18f:.40f,airborne?.65f:1.15f}) {
        const StorylandNativePoint start{position.x,position.y,position.z+height};
        const StorylandNativePoint end{start.x+forwardX*maximumProbe,
            start.y+forwardY*maximumProbe,start.z};
        const float hit=storylandTraceNativeSegment(world,start,end);
        if(hit>=.99f)continue;
        wallFound=true;
        wallDistance=std::min(wallDistance,std::max(0.0f,hit)*maximumProbe);
    }
    if(!wallFound)return false;

    constexpr float maximumRise=3.30f;
    const float minimumRise=airborne?.08f:.32f;
    // Use the first clear height above the contacted wall. A single ray from
    // maximum reach sees the top of an awning instead of the usable lip below.
    for(float probeHeight=minimumRise+.16f;probeHeight<=maximumRise+.20f;probeHeight+=.16f) {
        const StorylandNativePoint head{position.x,position.y,position.z+probeHeight};
        const StorylandNativePoint lipProbe{head.x+forwardX*(wallDistance+.04f),
            head.y+forwardY*(wallDistance+.04f),head.z};
        if(storylandTraceNativeSegment(world,head,lipProbe)<.99f)continue;
        // A radius-wide first offset skipped the entire cap of ordinary thin walls.
        for(float forward=std::max(.04f,wallDistance+.02f);forward<=3.20f;forward+=.08f) {
            const StorylandNativePoint top{head.x+forwardX*forward,head.y+forwardY*forward,head.z};
            if(storylandTraceNativeSegment(world,head,top)<.99f)continue;
            const StorylandNativePoint bottom{top.x,top.y,position.z+.06f};
            uint8_t surface=0;StorylandNativePoint normal{};
            const float hit=storylandTraceNativeGround(world,top,bottom,surface,&normal);
            if(hit>=1 || normal.z<.70f)continue;
            const float ledgeZ=top.z+(bottom.z-top.z)*hit;
            const float rise=ledgeZ-position.z;
            if(rise<minimumRise || rise>maximumRise)continue;
            const StorylandNativePoint ledge{position.x+forwardX*wallDistance,
                position.y+forwardY*wallDistance,ledgeZ};
            const auto edge=ledgeOffset(ledge,headingDegrees,EdgeHorizontal,
                std::max(position.z,ledgeZ-PedOriginAboveFeet)-ledgeZ);
            // Start the jump-and-grab as soon as the hanging feet clear the
            // approach floor; otherwise the high ledge uses the short mantle.
            const bool stretch=rise>PedOriginAboveFeet-HangingVertical;
            const auto hang=ledgeOffset(ledge,headingDegrees,HangingHorizontal,
                HangingVertical-PedOriginAboveFeet);
            StorylandNativePoint destination=ledgeOffset(ledge,headingDegrees,StandHorizontal,0);
            StorylandNativePoint supported{};
            const bool standSupported=supportAt(world,destination,.08f,.20f,supported) &&
                std::fabs(supported.z-ledgeZ)<.05f &&
                storylandNativeBodyClear(world,supported,climbBody(headingDegrees));

            bool vault=false;
            // The short-wall branch uses SA's separate vault clearance volume.
            // Finish beyond the lip on a supported, clear body position.
            if(!stretch && rise<=MinimumStretchGrab) {
                const auto beyond=ledgeOffset(ledge,headingDegrees,1.15f,0);
                StorylandNativePoint farFloor{};
                if(supportAt(world,beyond,.08f,rise+1.0f,farFloor) &&
                    farFloor.z<ledgeZ-.40f && farFloor.z>=position.z-1.0f &&
                    storylandNativeBodyClear(world,farFloor,climbBody(headingDegrees)) &&
                    climbPoseClear(world,ledge,headingDegrees,position.z,true)) {
                    destination=farFloor;vault=true;
                }
            }
            if(!vault && (!standSupported ||
                !climbPoseClear(world,ledge,headingDegrees,position.z,false)))continue;
            const auto approach=stretch?hang:edge;
            const auto move=storylandMoveNativeBody(world,position,approach,climbBody(headingDegrees));
            if(!move.clear || move.blocked || move.recovered)continue;
            state.climbStart=position;
            state.climbLedge=ledge;
            state.climbHang=hang;
            state.climbEdge=edge;
            state.climbDestination=vault?destination:supported;
            state.climbHeading=headingDegrees;
            state.climbVault=vault;
            state.climbSideLip=false;
            state.climbAligning=!stretch;
            state.climbPullRequested=false;
            state.climbDropRequested=false;
            state.crouched=false;
            state.justLanded=false;
            changeAction(state,stretch?StorylandExtendedAction::ClimbJump:
                vault?StorylandExtendedAction::ClimbVault:StorylandExtendedAction::ClimbStand);
            return true;
        }
    }
    // Fences and other thin collision meshes may have vertical faces without
    // an upward-facing cap. Find their upper edge from the last blocked and
    // first clear horizontal rays, then vault only when the far side has a
    // safe, supported landing. This does not turn an unsupported cliff into
    // a climbable platform.
    for(float high=minimumRise+.16f;high<=maximumRise+.20f;high+=.16f) {
        const auto traceAt=[&](float height) {
            const StorylandNativePoint start{position.x,position.y,position.z+height};
            const StorylandNativePoint end{start.x+forwardX*(wallDistance+.08f),
                start.y+forwardY*(wallDistance+.08f),start.z};
            return storylandTraceNativeSegment(world,start,end);
        };
        if(traceAt(high)<.99f || traceAt(high-.16f)>=.99f)continue;
        float low=high-.16f;
        for(unsigned step=0;step<12;++step) {
            const float middle=(low+high)*.5f;
            if(traceAt(middle)<.99f)low=middle;else high=middle;
        }
        const float rise=high;
        if(rise<minimumRise || rise>maximumRise)continue;
        const StorylandNativePoint ledge{position.x+forwardX*wallDistance,
            position.y+forwardY*wallDistance,position.z+rise};
        if(!sideLipPresent(world,ledge,headingDegrees))continue;
        const auto landingProbe=ledgeOffset(ledge,headingDegrees,1.15f,0);
        StorylandNativePoint landing{};
        if(!supportAt(world,landingProbe,.08f,rise+1.0f,landing) ||
            landing.z>=ledge.z-.40f || landing.z<position.z-1.0f ||
            !storylandNativeBodyClear(world,landing,climbBody(headingDegrees)) ||
            !climbPoseClear(world,ledge,headingDegrees,position.z,true))continue;
        const bool stretch=rise>MinimumStretchGrab;
        const auto hang=ledgeOffset(ledge,headingDegrees,HangingHorizontal,
            std::max(position.z,ledge.z+HangingVertical-PedOriginAboveFeet)-ledge.z);
        const auto edge=ledgeOffset(ledge,headingDegrees,EdgeHorizontal,
            std::max(position.z,ledge.z-PedOriginAboveFeet)-ledge.z);
        const auto approach=stretch?hang:edge;
        const auto move=storylandMoveNativeBody(world,position,approach,climbBody(headingDegrees));
        if(!move.clear || move.blocked || move.recovered)continue;
        state.climbStart=position;state.climbLedge=ledge;state.climbHang=hang;
        state.climbEdge=edge;state.climbDestination=landing;state.climbHeading=headingDegrees;
        state.climbVault=true;state.climbSideLip=true;state.climbAligning=!stretch;
        state.climbPullRequested=false;state.climbDropRequested=false;
        state.crouched=false;state.justLanded=false;
        changeAction(state,stretch?StorylandExtendedAction::ClimbJump:StorylandExtendedAction::ClimbVault);
        return true;
    }
    return false;
}

bool storylandStepExtendedMovement(StorylandExtendedMovement& state,
    StorylandPlayTestRuntime& runtime, const StorylandNativeWorld& world,
    StorylandNativePoint& position, float headingDegrees, float seconds) {
    if(!state.busy() || !std::isfinite(seconds) || seconds<=0)return false;
    float remaining=std::min(seconds,.1f);
    if(state.climbing()) {
        StorylandNativePoint ledgeSupport{};
        const auto contact=ledgeOffset(state.climbLedge,state.climbHeading,.02f,0);
        const bool lipValid=state.climbSideLip?sideLipPresent(world,state.climbLedge,state.climbHeading):
            (supportAt(world,contact,.08f,.20f,ledgeSupport) &&
             std::fabs(ledgeSupport.z-state.climbLedge.z)<=.05f);
        if(state.climbDropRequested || !lipValid || !climbPoseClear(world,state.climbLedge,
            state.climbHeading,state.climbStart.z,state.climbVault)) {
            dropClimb(state,runtime,world,position);return true;
        }
        runtime.movementSpeed=0;runtime.grounded=false;
        runtime.verticalSpeed=0;runtime.airborneTime=0;
        if(state.climbAligning) {
            // SA holds the first stand/vault frame until its ped root reaches
            // the edge anchor. Convert that anchor to Storyland's feet origin.
            const float fraction=1.0f-std::exp(-8.630462f*remaining);
            const auto next=interpolation(position,state.climbEdge,fraction);
            if(!moveApproach(world,position,next,state.climbHeading)) {
                dropClimb(state,runtime,world,position);return true;
            }
            if(std::hypot(std::hypot(position.x-state.climbEdge.x,
                position.y-state.climbEdge.y),position.z-state.climbEdge.z)>.025f)return true;
            position=state.climbEdge;state.climbAligning=false;
            return true;
        }
        if(state.action==StorylandExtendedAction::ClimbIdle) {
            position=state.climbHang;
            if(!state.climbPullRequested) {
                state.elapsed=std::fmod(state.elapsed+remaining,
                    storylandExtendedActionDuration(StorylandExtendedAction::ClimbIdle));
                return true;
            }
            changeAction(state,StorylandExtendedAction::ClimbPull);
        }
    }
    while(remaining>0 && state.busy()) {
        const auto action=state.action;
        const float duration=storylandExtendedActionDuration(action);
        const float available=std::max(0.0f,duration-state.elapsed);
        const float dt=std::min(remaining,available);
        const float begin=state.elapsed;
        state.elapsed+=dt;remaining-=dt;
        const float to=std::clamp(state.elapsed/std::max(.001f,duration),0.0f,1.0f);
        if(state.rolling() && dt>0) {
            if(!runtime.extendedRollRootDelta) {state.cancel();return false;}
            const auto root=runtime.extendedRollRootDelta(action,begin,state.elapsed);
            if(!std::isfinite(root.x)||!std::isfinite(root.y)||!std::isfinite(root.z)) {
                state.cancel();return false;
            }
            const float rotation=headingDegrees*Radians;
            // Converted SA animations face local+Y. Consume both authored
            // horizontal channels; vertical curl remains in the pose.
            const auto before=position;
            const StorylandNativePoint next{position.x+std::cos(rotation)*root.y+std::sin(rotation)*root.x,
                position.y+std::sin(rotation)*root.y-std::cos(rotation)*root.x,position.z};
            const auto move=storylandMovePlayTestCharacter(world,runtime,position,next);
            if(move.clear)position=move.position;
            runtime.movementSpeed=std::hypot(root.x,root.y)/dt;
            StorylandNativePoint floor{};
            if(!supportAt(world,position,.08f,.35f,floor)) {
                state.cancel();runtime.grounded=false;
                runtime.verticalSpeed=std::min(0.0f,runtime.verticalSpeed);
                runtime.jumpHorizontalVelocity={(position.x-before.x)/dt,(position.y-before.y)/dt};
                runtime.jumpHorizontalValid=true;runtime.jumpFromLaunch=false;
                runtime.playerFall={};return true;
            }
            position.z=floor.z;
        } else if(state.climbing()) {
            StorylandNativePoint next=position;
            const float progress=rootProgress(runtime,action,state.elapsed,to);
            if(action==StorylandExtendedAction::ClimbJump)
                next=interpolation(state.climbStart,state.climbHang,to);
            else if(action==StorylandExtendedAction::ClimbPull)
                next=interpolation(state.climbHang,state.climbEdge,progress);
            else if(action==StorylandExtendedAction::ClimbStand || action==StorylandExtendedAction::ClimbVault)
                next=interpolation(state.climbEdge,state.climbDestination,progress);
            if(action==StorylandExtendedAction::ClimbJump || action==StorylandExtendedAction::ClimbPull) {
                if(!moveApproach(world,position,next,state.climbHeading)) {
                    dropClimb(state,runtime,world,position);return true;
                }
            } else position=next;
        }
        if(state.elapsed+0.000001f<duration)return true;
        switch(action) {
        case StorylandExtendedAction::ClimbJump:
            changeAction(state,state.climbPullRequested?StorylandExtendedAction::ClimbPull:
                StorylandExtendedAction::ClimbIdle);break;
        case StorylandExtendedAction::ClimbIdle: return true;
        case StorylandExtendedAction::ClimbPull:
            changeAction(state,state.climbVault?StorylandExtendedAction::ClimbVault:
                StorylandExtendedAction::ClimbStand);break;
        case StorylandExtendedAction::ClimbStand:
            changeAction(state,StorylandExtendedAction::ClimbFinish);break;
        case StorylandExtendedAction::ClimbVault:
            changeAction(state,StorylandExtendedAction::ClimbVaultFinish);break;
        case StorylandExtendedAction::ClimbFinish:
        case StorylandExtendedAction::ClimbVaultFinish: {
            const bool vault=state.climbVault;
            StorylandNativePoint floor{};
            if(!storylandNativeBodyClear(world,state.climbDestination,climbBody(state.climbHeading)) ||
                (!vault && (!supportAt(world,state.climbDestination,.08f,.20f,floor) ||
                    std::fabs(floor.z-state.climbDestination.z)>.05f))) {
                dropClimb(state,runtime,world,position);return true;
            }
            position=state.climbDestination;
            state.cancel();
            runtime.grounded=!vault;
            runtime.verticalSpeed=vault?-.5f:0;
            runtime.airborneTime=0;runtime.landingTime=0;
            break;
        }
        default: state.cancel();break;
        }
        if(state.action==StorylandExtendedAction::ClimbIdle)return true;
    }
    return true;
}

void storylandCancelExtendedMovement(StorylandPlayTestRuntime& runtime, bool clearCrouch) {
    runtime.extendedMovement.cancel();
    if(clearCrouch) {
        auto& state=runtime.extendedMovement;
        state.crouched=false;state.crouchStanceBlend=0;state.crouchMoveBlend=0;
        state.crouchAnimationTime=0;state.crouchMoveAmount=0;state.crouchMoveCommand=0;
        state.crouchTravelDistance=0;state.crouchMotionDirection=0;
    }
}
