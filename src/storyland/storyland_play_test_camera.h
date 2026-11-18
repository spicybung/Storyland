#pragma once
#include "src/storyland/storyland_play_test.h"
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_native_body.h"

struct StorylandMissionEnvironment;
struct StorylandRuntimeVehicle;

struct StorylandSpeedCamera {
    StorylandNativePoint shake{};
    StorylandNativePoint aimShift{};
    uint8_t blur=0;
};

struct StorylandVehicleCameraInput {
    StorylandPlayTestCameraInput look{};
    bool lookBehind=false,lookLeft=false,lookRight=false;
    bool scriptedZoom=false;
    float zoom=0;
    // The physical pool slot distinguishes ambient vehicles without script handles.
    int32_t vehicleSlot=-1;
};

struct StorylandVehicleCameraState {
    bool initialized=false;
    int32_t model=-1,handle=-1,vehicleSlot=-1,lastMode=-1;
    float zoom=0,betaSpeed=0,alphaSpeed=0,manualSeconds=0,fieldOfView=70;
    StorylandNativePoint historyFar{},historyNear{},lastPosition{};
    float lastHeading=0;
    unsigned cinematicIndex=0;
    int32_t cinematicMode=-1;
    float cinematicSeconds=0;
    StorylandNativePoint cinematicEye{};
};

struct StorylandStoriesCamera {
    bool initialized = false, refocusing = false;
    float yaw = 0, pitch = 0, distance = 3.5f,requestedDistance=3.5f;
    unsigned vehicleView=1;
    int32_t vehicleMode=2;
    bool vehicleCameraActive=false;
    StorylandNativePoint vehicleUp{0,0,1};
    float vehicleNearClip=.9f;
    StorylandVehicleCameraState vehicleController;
    bool aimLocked=false;
    StorylandNativePoint aimTarget{};
    // Per-frame native lock-camera inputs supplied by the rendered player,
    // decoded weapon and viewport. Missing inputs do not synthesize a pivot.
    StorylandNativePoint aimPlayerRoot{};
    bool aimPlayerRootValid=false;
    StorylandNativePoint aimTargetRoot{};
    bool aimTargetRootValid=false;
    int32_t aimNativeWeapon=-1,aimFireType=-1;
    float aspectRatio=0;
    bool nativeLockActive=false, nativeFollowPedActive=false;
    float nativeLockFieldOfView=70;
    StorylandNativePoint target, eye, stringEye;
    // Runtime view only; persistent script intent lives in the mission world.
    bool manualAimActive=false,manualAimHit=false;
    bool freeAimActive=false;
    int32_t manualAimMode=0;
    // Vehicle cameras keep the native horizontal FOV. Projection/culling
    // convert it once using the actual viewport aspect ratio.
    float fieldOfView=70;
    StorylandSpeedCamera speedEffects;
    float speedMotionIntensity=0;
    StorylandNativePoint manualAimHitPoint{};
};

struct StorylandManualAimInput {
    StorylandPlayTestCameraInput look{};
    StorylandNativePoint head{},torso{},shoulder{};
    bool headValid=false,torsoValid=false,shoulderValid=false;
    bool zoomIn=false,zoomOut=false,controlEnabled=true,suspended=false;
    bool cameraAiming=false;
    int32_t weaponMode=0;
    int32_t nativeWeapon=-1;
    float weaponRange=0;
};

int32_t storylandWeaponManualAimMode(int32_t nativeWeapon);
bool storylandPlayerSeatedDriveByVehicle(const StorylandMissionEnvironment& environment,
    int32_t nativeWeapon,std::array<float,16>* matrix=nullptr);
StorylandNativePoint storylandSeatedDriveByLocalTarget(const StorylandMissionEnvironment& environment,
    const std::array<float,16>& matrix,bool manualActive);
bool storylandUpdateManualAimCamera(StorylandStoriesCamera& camera,StorylandMissionEnvironment& environment,
    const StorylandNativePoint& player,float heading,const StorylandManualAimInput& input,
    const StorylandNativeWorld& world);
bool storylandUpdateFreeAimCamera(StorylandStoriesCamera& camera,
    const StorylandNativePoint& player,const StorylandNativePoint& anchor,float heading,
    const StorylandPlayTestCameraInput& input,float seconds,float range,
    const StorylandNativeWorld& world);
std::array<float,3> storylandSampleCameraShake(StorylandMissionEnvironment& environment);
StorylandSpeedCamera storylandVehicleSpeedCamera(bool vcs,int vehicleType,float speed,
    bool eligible,uint32_t random);
void storylandUpdateSpeedCamera(StorylandStoriesCamera& camera,StorylandMissionEnvironment& environment,
    bool vcs,int vehicleType,float speed,bool eligible);
void storylandUpdateStoriesSpeedCamera(StorylandStoriesCamera& camera,
    StorylandMissionEnvironment& environment,const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,float seconds,bool eligible);
void storylandResetVehicleSpeedCamera(StorylandStoriesCamera& camera);
void storylandApplySpeedCamera(StorylandStoriesCamera& camera);
void storylandApplyCameraShake(StorylandStoriesCamera& camera,StorylandMissionEnvironment& environment);
void storylandApplyScriptViewEffects(StorylandStoriesCamera& camera,StorylandMissionEnvironment& environment);
bool storylandShakeCameraAt(StorylandMissionEnvironment& environment,
    const StorylandNativePoint& position,float strength);
bool storylandCameraSphereVisible(const StorylandStoriesCamera& camera,const StorylandNativePoint& point,
    float radius,float aspect,float nearClip,float farClip,const StorylandNativePoint& up={0,0,1});
bool storylandCameraScreenCoordinates(const StorylandStoriesCamera& camera,const StorylandNativePoint& point,
    float aspect,std::array<float,2>& screen,const StorylandNativePoint& up={0,0,1});
void storylandUpdateScriptCameraFocus(StorylandMissionEnvironment& environment);
float storylandScriptCameraDistance(const StorylandMissionEnvironment& environment,float base,bool vehicle);
int32_t storylandCycleVehicleCameraMode(int32_t current,int direction=1,int32_t model=-1);
bool storylandUpdateVehicleCamera(StorylandStoriesCamera& camera,
    const StorylandRuntimeVehicle& vehicle,const StorylandNativeTrafficActor& actor,
    const StorylandVehicleCameraInput& input,float seconds,const StorylandNativeWorld& world);

void storylandResetStoriesCamera(StorylandStoriesCamera& camera,
    const StorylandNativePoint& player, float heading);
bool storylandResetStoriesCameraForTransition(StorylandStoriesCamera& camera,
    const StorylandNativePoint& player, float heading, bool preserveAim);
void storylandUpdateStoriesCamera(StorylandStoriesCamera& camera,
    const StorylandNativePoint& player, float heading,
    const StorylandPlayTestCameraInput& input, float seconds,
    const StorylandNativeWorld& world, bool collisionEnabled=true,
    bool nativeFollowPed=false);
// Returns the unobstructed fraction of the camera segment, with near-plane clearance.
float storylandStoriesCameraVisibility(const StorylandNativeWorld& world,
    const StorylandNativePoint& target, const StorylandNativePoint& eye);

float storylandTraceNativeSegment(const StorylandNativeWorld& world,
    const StorylandNativePoint& start,const StorylandNativePoint& end);
// Keep invisible collision barriers; callers must resolve visible script-object hits separately.
float storylandTraceNativeSegmentWithoutVisibleScriptObjects(const StorylandNativeWorld& world,
    const StorylandNativePoint& start,const StorylandNativePoint& end);
float storylandTraceNativeSegmentCategories(const StorylandNativeWorld& world,
    const StorylandNativePoint& start, const StorylandNativePoint& end,
    bool buildings, bool dummies);
float storylandTraceNativeSectorSegment(const StorylandNativeWorld& world,
    const StorylandNativePoint& start,const StorylandNativePoint& end,uint16_t sectorLists);

float storylandTraceNativeGround(const StorylandNativeWorld& world,const StorylandNativePoint& start,
    const StorylandNativePoint& end,uint8_t& surface,StorylandNativePoint* normal=nullptr);
float storylandTraceNativeVehicleGround(const StorylandNativeWorld& world,const StorylandNativePoint& start,
    const StorylandNativePoint& end,uint8_t& surface,StorylandNativePoint* normal=nullptr);
float storylandTraceNativeMapGround(const StorylandNativeWorld& world,const StorylandNativePoint& start,
    const StorylandNativePoint& end);
float storylandTraceNativeTrafficSurface(const StorylandNativeWorld& world,const StorylandNativePoint& start,
    const StorylandNativePoint& end,uint8_t& surface,StorylandNativePoint& normal);
float storylandTraceNativeBuildings(const StorylandNativeWorld& world,const StorylandNativePoint& start,
    const StorylandNativePoint& end);
