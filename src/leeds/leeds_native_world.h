#pragma once
#include <array>
#include "src/storyland/storyland_road_control.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

struct StorylandNativePoint {
    float x = 0, y = 0, z = 0;
};
struct StorylandVcsZonePopulationRecord {
    uint16_t carDensity=0;
    std::array<uint16_t,13> vehicleClassThresholds{};
    std::array<uint16_t,9> gangCarThresholds{};
    uint16_t policeCarThreshold=0;
    uint16_t pedestrianDensity=0;
    std::array<uint16_t,9> gangPedestrianThresholds{};
    uint16_t policePedestrianThreshold=0;
    uint16_t pedestrianGroup=0;
};
struct StorylandNativeZone {
    std::string name;
    StorylandNativePoint minimum, maximum;
    uint32_t type = 0, level = 0;
    int32_t child=-1,next=-1;
    uint16_t populationDay=0,populationNight=0;
    bool populationLoaded = false;
    std::array<StorylandVcsZonePopulationRecord,2> populationRecords{};

    bool contains(const StorylandNativePoint& point) const;
};
struct StorylandNativeZonePopulation {
    // Native densities: total, nine individual gangs, police. Thresholds in
    // CZoneInfo are expanded to independent weights for script overrides.
    std::array<int32_t,11> cars{},pedestrians{};
    std::array<int32_t,9> civilianCars{};
    std::array<int32_t,2> boats{};
    uint16_t pedestrianGroup=0;
};
struct StorylandNativeZones {
    std::vector<StorylandNativeZone> navigation, information;
    std::vector<StorylandNativeZone> map;
    std::vector<uint16_t> audio;
    std::vector<StorylandNativeZonePopulation> populations;
    bool vcs=true;
    std::string status;

    const StorylandNativeZone* named(const std::string& name) const;
    const StorylandNativeZone* at(const StorylandNativePoint& point, bool informationZone) const;
    uint32_t levelAt(const StorylandNativePoint& point) const;
    uint32_t worldLevelAt(const StorylandNativePoint& point,uint32_t currentLevel=0) const;
};
bool storylandDecodeNativeZones(const std::vector<uint8_t>& data, bool vcs,
    StorylandNativeZones& zones, std::string& error);
struct StorylandNativePathLink {
    uint32_t target = 0;
    uint8_t distance = 0;
    bool trafficLight = false;
    bool crossesRoad = false;
    bool nativeLanes = false;
    uint8_t lanes = 1, oppositeLanes = 1;
    uint16_t carLink = UINT16_MAX;
    uint8_t signalGroup=0;
    bool signalApplies=false;
    float dividerWidth=0;
    bool nativeStopLine=false;
    StorylandNativePoint stopPosition,stopDirection;
};
struct StorylandNativePathNode {
    StorylandNativePoint position;
    uint16_t flags = 0;
    uint8_t width = 0;
    uint8_t speedLimit = 0;
    uint8_t spawnRate = 0;
    bool vehicle = false;
    bool water = false;
    bool disabled = false;
    std::vector<StorylandNativePathLink> links;
};
struct StorylandNativePaths {
    bool vcs=false;
    std::vector<StorylandNativePathNode> nodes;
    // VCS FindNodeClosestToCoors walks y, then x, then the retained cell list.
    // Keeping that order matters for equidistant nodes and its100-unit first pass.
    std::vector<std::array<std::vector<uint16_t>,2>> vcsSearchGrid;
    std::vector<std::array<std::vector<uint16_t>,2>> lcsSearchGrid;
    uint32_t vehicleNodeCount = 0;
    uint32_t pedestrianNodeCount = 0;
    uint32_t connectionCount = 0;
    std::string status;
};
struct StorylandNativeCollisionTriangle {
    std::array<uint32_t, 3> vertices{};
    uint8_t surface = 0;
};
struct StorylandNativeCollisionBox {
    StorylandNativePoint minimum, maximum;
    uint8_t surface = 0, piece = 0;
};
struct StorylandNativeCollisionSphere {
    StorylandNativePoint center;
    float radius = 0;
    uint8_t surface = 0, piece = 0;
};
// Store the COL shapes in local space. Many world instances use the same model.
struct StorylandNativeCollisionModel {
    StorylandNativePoint minimum, maximum;
    StorylandNativePoint boundingCenter;
    float boundingRadius=0;
    std::vector<StorylandNativePoint> vertices;
    std::vector<StorylandNativeCollisionTriangle> triangles;
    std::vector<StorylandNativeCollisionBox> boxes;
    std::vector<StorylandNativeCollisionSphere> spheres;
    uint8_t collisionSlot=0xFF;
};
struct StorylandBuildingSwap {
    uint32_t buildingIndex=0,hash=0,state=0;
    uint32_t originalModel=0,replacementModel=0,group=0;
};
struct StorylandNativeCollisionInstance {
    uint32_t poolHeader=0,poolIndex=0,originalModelId=0;
    uint32_t modelId = 0;
    uint16_t nativeSectorLists=0x3FFF;
    bool nativeUsesCollision=true;
    std::array<float, 16> matrix{};
    std::array<float, 16> originalMatrix{};
};
struct StorylandNativeWater {
    float west=0, east=0, south=0, north=0, height=0;
};
struct StorylandNativeCollisionBank {
    uint8_t slot=0;
    std::string name;
    float left=0,top=0,right=0,bottom=0;
    bool resident=false;
    bool contains(float x,float y,float margin=0) const {
        return x>=left+margin&&x<=right-margin&&y>=bottom+margin&&y<=top-margin;
    }
};
struct StorylandMissionEnvironment;
struct StorylandNativeWorld {
    const StorylandMissionEnvironment* scriptEnvironment = nullptr;
    StorylandNativeZones zones;
    std::vector<StorylandNativeWater> water;

    StorylandNativePaths paths;
    std::map<uint32_t, StorylandNativeCollisionModel> collisionModels;
    std::vector<StorylandNativeCollisionInstance> instances;
    std::map<uint64_t,std::array<float,16>> originalScriptInstanceMatrices;
    std::map<uint8_t, uint64_t> surfaces;
    std::string collisionStatus;
    std::map<std::pair<int32_t,int32_t>,std::vector<uint32_t>> collisionCells;
    std::vector<uint32_t> collisionLargeInstances;
    const StorylandNativeCollisionInstance* collisionIndexOwner=nullptr;
    size_t collisionIndexCount=0,collisionIndexModels=0;
    int32_t fortStauntonCollisionSlot=-1,fortDestroyedCollisionSlot=-1;
    bool fortStauntonDestroyed=false;
    std::vector<StorylandNativeCollisionBank> collisionBanks;
    std::set<uint8_t> activeCollisionSlots,requestedCollisionSlots;
    bool collisionStreamingActive=false;
    bool collisionModelEnabled(const StorylandNativeCollisionModel& model) const {
        return (int32_t(model.collisionSlot)!=fortStauntonCollisionSlot||!fortStauntonDestroyed)&&
            (int32_t(model.collisionSlot)!=fortDestroyedCollisionSlot||fortStauntonDestroyed)&&
            (!collisionStreamingActive||model.collisionSlot==0||model.collisionSlot==0xFF||
                activeCollisionSlots.count(model.collisionSlot)!=0);
    }
};
inline constexpr float StorylandCollisionCellSize=64.0f;
void storylandPrepareNativeCollisionIndex(StorylandNativeWorld& world);
bool storylandSetNativeInstanceMatrix(StorylandNativeWorld& world,size_t index,
    const std::array<float,16>& matrix);
void storylandResetBuildingSwaps(StorylandNativeWorld& world);
bool storylandReplaceNativeBuildingModel(StorylandNativeWorld& world,uint64_t worldKey,uint32_t model);
bool storylandSetNativeInstanceTransform(StorylandNativeWorld& world,uint64_t worldKey,const std::array<float,16>& matrix);
void storylandSetFortStauntonDestroyed(StorylandNativeWorld& world,bool destroyed);
bool storylandRequestNativeCollision(StorylandNativeWorld& world,const StorylandNativePoint& position,
    int32_t fallbackLevel,std::string& error);
bool storylandLoadNativeSceneCollision(StorylandNativeWorld& world,const StorylandNativePoint& position,
    int32_t fallbackLevel,const std::array<int32_t,2>& allowedLevels,bool enabled,std::string& error);
void storylandSetNativeCollisionLevels(StorylandNativeWorld& world,int32_t level,
    const std::array<int32_t,2>& allowedLevels,bool enabled);
void storylandResetNativeCollisionStreaming(StorylandNativeWorld& world);
size_t storylandApplyBuildingSwaps(StorylandNativeWorld& world,
    const std::vector<StorylandBuildingSwap>& swaps,const std::map<uint32_t,int32_t>& states);
int32_t storylandBuildingSwapHandle(const StorylandNativeWorld& world,
    const std::vector<StorylandBuildingSwap>& swaps,uint32_t hash);
bool storylandBuildingSwapBounds(const StorylandNativeWorld& world,
    const std::vector<StorylandBuildingSwap>& swaps, uint32_t hash,
    std::array<float,3>& center, float& radius);
struct StorylandNativeTrafficActor {
    uint32_t node = 0, nextNode = 0, previousNode = UINT32_MAX;
    uint32_t routeSeed = 0;
    float distance = 0,groundOffset=0;
    float laneOffset=0;
    uint8_t lane=0;
    float blockedSeconds=0;
    bool crossingCommitted=false;
    bool vehicle = false;
    bool water = false;
    bool avoidLevelTransitions=false; // Derived from the mission vehicle's native autopilot flag.
    std::vector<uint32_t> destinationRoute;
    size_t destinationRouteCursor=0;
    uint64_t destinationRoadRevision=0;
    float destinationRetryAfter=0;
    StorylandNativePoint destinationRouteTarget;
    bool destinationRouteValid=false, destinationRouteFinished=false;
    bool destinationAvoidLevelTransitions=false;
    StorylandNativePoint position;
    float yaw = 0;
};

bool storylandDecodeNativePaths(const std::vector<uint8_t>& data, bool vcs,
    StorylandNativePaths& paths, std::string& error);
bool storylandDecodeNativeCollisionModel(const std::vector<uint8_t>& data, uint32_t offset,
    uint32_t limit, bool vcs, StorylandNativeCollisionModel& model, std::string& error);
bool storylandDecodeNativeCol2(const std::vector<uint8_t>& data, bool vcs,
    std::map<uint32_t, StorylandNativeCollisionModel>& models, std::string& error);
void storylandDecodeNativeInstances(const std::vector<uint8_t>& data, bool vcs,
    std::vector<StorylandNativeCollisionInstance>& instances);
std::array<float, 3> storylandCollisionSurfaceColor(uint8_t surface);
std::vector<StorylandNativeTrafficActor> storylandCreateNativeTraffic(
    const StorylandNativePaths& paths, const StorylandNativePoint& origin, bool vehicle, size_t maximum,
    float minimumDistance = 10.0f, float maximumDistance = 250.0f, bool water = false);
void storylandAdvanceNativeTraffic(const StorylandNativePaths& paths,
    StorylandNativeTrafficActor& actor, float seconds,
    const StorylandNativeZones* zones=nullptr, bool avoidLevelTransitions=false,
    bool fastLane=false, bool ignoreDisabledRoads=false);
bool storylandNativeTrafficRoutePosition(const StorylandNativePaths& paths,
    const StorylandNativeTrafficActor& actor, StorylandNativePoint& position,
    const StorylandNativeZones* zones=nullptr, bool avoidLevelTransitions=false,
    bool ignoreDisabledRoads=false);
bool storylandJoinNativeVehicleRoute(const StorylandNativePaths& paths,
    StorylandNativeTrafficActor& actor, const StorylandNativeZones* zones=nullptr,
    bool avoidLevelTransitions=false);
bool storylandNativeVehiclePathCoordinate(const StorylandNativePaths& paths,
    const StorylandNativeTrafficActor& actor, int32_t index, StorylandNativePoint& point,
    const StorylandNativeZones* zones=nullptr, bool avoidLevelTransitions=false);
bool storylandFindPlayTestDtz(const std::wstring& worldPath, std::wstring& dtzPath, std::string& error);
struct StorylandEmbeddedCollision {
    uint32_t offset = 0;
    StorylandNativeCollisionModel model;
};
std::vector<StorylandEmbeddedCollision> storylandDecodeEmbeddedMdlCollision(const std::vector<uint8_t>& bytes);

void storylandPrepareNativeWater(const std::vector<uint8_t>& data,bool vcs,StorylandNativeWorld& world);
bool storylandNativeWaterHeight(const StorylandNativeWorld& world,float x,float y,float& height);
bool storylandNativeWaterSegment(const StorylandNativeWorld& world,
    const StorylandNativePoint& from,const StorylandNativePoint& to);

float storylandNativeTrafficTurn(const StorylandNativePaths& paths,const StorylandNativeTrafficActor& actor,
    float& remaining,bool ignoreDisabledRoads=false);

size_t storylandApplyNativeRoadChanges(StorylandNativePaths& paths,
    const std::vector<StorylandRoadChange>& changes,size_t applied=0);
std::vector<uint8_t> storylandNativeLevelTransitionNodes(const StorylandNativePaths& paths,
    const std::vector<StorylandRoadChange>& changes);
std::vector<uint16_t> storylandNativeRoadNodeFlags(const StorylandNativePaths& paths,
    const std::vector<StorylandRoadChange>& changes);
