#pragma once
#include <cstdint>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "src/leeds/leeds_texture.h"
#include "src/storyland/storyland_visibility.h"
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_shared_buffer.h"

class StorylandDtzArchive;
class StorylandVcsModloader;

struct StorylandNativeSceneryReport {
    size_t modelCount = 0;
    size_t placementCount = 0;
    size_t textureCount = 0;
    size_t emptyModelCount = 0;
    std::vector<std::string> skippedModels;
    std::vector<std::string> missingTextures;
    std::string summary() const;
};

struct StorylandArchiveEntry {
    uint32_t index = 0;
    uint32_t startSector = 0;
    uint32_t sectorCount = 0;
    uint64_t byteOffset = 0;
    uint64_t byteSize = 0;
    uint32_t lvzHeaderOffset = 0;
    uint32_t chunkIdent = 0;
    bool usesLvzChunkHeader = false;
    std::string name;
    std::vector<std::string> textureNames;
};

struct StorylandWorldConditionalMembership {
    uint32_t sectorIndex = 0;
    uint8_t selector = 0;
    uint8_t state = 0;
};

struct StorylandWorldPlacement {
    uint32_t resourceIndex = 0;
    uint32_t iplId = 0;
    uint32_t iplRaw = 0;
    uint32_t sectorIndex = 0;
    uint32_t sectorX = 0;
    uint32_t sectorY = 0;
    uint32_t passIndex = 0;
    uint64_t imgOffset = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float boundX = 0.0f;
    float boundY = 0.0f;
    float boundZ = 0.0f;
    float boundRadius = 1.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float scaleZ = 1.0f;
    float matrix[16] = {};
    std::string passName;
    bool compiledSectorVisibility = false;
    bool nativeSectorSelection = false;
    std::vector<uint32_t> visibilitySectors;
    std::vector<StorylandWorldConditionalMembership> conditionalVisibilitySectors;
    uint32_t nativeInstanceIndex = UINT32_MAX;
    uint32_t nativePoolHeader = 0;
    uint32_t nativePoolIndex = UINT32_MAX;
    uint32_t nativeEntityFlags = 0;
    uint32_t nativeEntityFlags2 = 0;
    uint16_t nativeModelFlags = 0;
    uint8_t nativeArea = 0;
    uint8_t nativeLevel = 0;
    uint8_t nativeAtomicIndex = 0;
    bool nativeVisible = true;
    bool nativeAreaRestricted = true;
    float nativeMinDrawDistance = 0;
    float nativeDrawDistance = 0;

};


// CPU copy of the decoded stream vertex, shared by the colour and shadow passes.
struct StorylandWorldMeshVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    uint8_t red = 255, green = 255, blue = 255, alpha = 255;
    bool hasPrelight = false;
    float nx=0, ny=0, nz=0;
    std::array<uint8_t, 4> ps2Color{128, 128, 128, 128};
    bool hasPs2Color = false;
    float ps2Nx=0, ps2Ny=0, ps2Nz=0;
};

struct StorylandWorldMeshTriangle {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t textureId = 0xFFFFFFFFu;
    uint32_t pspStripIndex = UINT32_MAX;
};

struct StorylandWorldPspStripState {
    float textureLodSlope = 1.0f;
    bool cullBackFaces = false;
    bool depthCenterOffset = false;
    bool modelSlope = false;
};

struct StorylandWorldMesh {
    uint32_t sectorIndex = 0;
    uint32_t resourceIndex = 0;
    // Retail AERA Resource[] identifies the original CBaseModelInfo entry.
    // Other WRLD resource namespaces do not imply a model-info identity.
    int32_t nativeModelInfoId = -1;
    uint64_t rawOffset = 0;
    uint64_t textureScopeOffset = uint64_t(-1);
    bool textureScopeInImg = true;
    int32_t textureScopeCellX = 0;
    int32_t textureScopeCellY = 0;
    bool hasTextureScopeCell = false;
    bool emptyGeometry = false;
    bool nativeVegetation = false;
    bool nativeAlphaTest = false;
    bool nativeAlphaBlend = false;
    bool nativeHasNormals = false;
    float nativeMaterialEffect = 0.0f;

    StorylandMeshBounds bounds;
    uint32_t materialCount = 0;
    StorylandSharedBuffer<StorylandWorldMeshVertex> vertices;
    StorylandSharedBuffer<StorylandWorldMeshTriangle> triangles;
    std::vector<StorylandWorldPspStripState> pspStrips;
};

// A compiled native prop can be instantiated by a script without becoming a
// permanent streamed-world placement. Both native passes share normalization.
struct StorylandLcsDynamicGeometry {
    struct Part {
        StorylandWorldMesh mesh;
        bool transparent = false;
    };
    std::array<float,3> scale{}, offset{};
    std::vector<Part> parts;
};

struct StorylandDirectTextureResource {
    uint32_t index = 0;
    uint32_t headerOffset = 0;
    uint32_t dataOffset = 0;
    uint32_t rasterFlags = 0;
    uint16_t widthHalf = 0;
    uint16_t formatFlags = 0;
    int width = 0;
    int height = 0;
    int bpp = 0;
    int32_t materialId = -1;
    bool unambiguousMaterialBinding = false;
    bool storedInImg = false;
    uint32_t baseOffset = 0;
    uint32_t scopeSectorIndex = 0xFFFFFFFFu;
    int32_t scopeCellX = 0;
    int32_t scopeCellY = 0;
    bool hasScopeCell = false;
    uint32_t storageBytes = 0;
    bool legacyRaw4bpp = false;
    bool pspRaster = false;
    std::string source;
    std::string name;
    StorylandSharedBuffer<uint8_t> rgba;
    std::vector<RgbaMipLevel> mipLevels;
    StorylandSharedBuffer<uint8_t> ps2Alpha;
    bool ps2FiveBitColor = false;
};

inline size_t storylandDirectTextureDecodedBytes(const StorylandDirectTextureResource& texture) {
    size_t bytes = rgbaDecodedByteTotal(texture.rgba.size(), texture.ps2Alpha.size());
    for (const auto& level : texture.mipLevels)
        bytes = rgbaDecodedByteTotal(bytes, rgbaMipLevelDecodedBytes(level));
    return bytes;
}

inline bool leedsBuildNativePs2Rgba(const StorylandDirectTextureResource& texture,
    std::vector<uint8_t>& nativeRgba, std::string& errorMessage) {
    return leedsBuildNativePs2Rgba(texture.rgba.data(), texture.rgba.size(),
        texture.ps2Alpha.data(), texture.ps2Alpha.size(), texture.ps2FiveBitColor,
        nativeRgba, errorMessage);
}

struct StorylandWorldSector {
    bool conditional=false;
    uint8_t swapGroup=0,swapState=0;
    uint32_t sectorIndex = 0;
    uint32_t sectorX = 0;
    uint32_t sectorY = 0;
    uint32_t headerOffset = 0;
    uint64_t imgOffset = 0;
    uint64_t byteSize = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
};

// A concrete row from a slave WRLD/AERA Resource[] table in the IMG.  Keep
// this separate from sGeomInstance.iplId: IPL ids identify instances while
// resourceId identifies the model payload referenced by an instance.
struct StorylandImgResourceRow {
    uint32_t sectorIndex = 0;
    uint32_t sectorX = 0;
    uint32_t sectorY = 0;
    uint32_t rowIndex = 0;
    uint32_t resourceId = 0;
    uint64_t tableOffset = 0;
    uint64_t payloadOffset = 0;
    uint64_t payloadSize = 0;
    int32_t secondaryId = -1;
    std::string layout;
    bool usedByPlacement = false;
    bool decodedAsMesh = false;
};

struct StorylandResourceResolution {
    uint32_t sectorIndex = 0;
    uint32_t sectorX = 0;
    uint32_t sectorY = 0;
    uint32_t resourceId = 0;
    uint32_t placementCount = 0;
    uint32_t candidateCount = 0;
    uint64_t payloadOffset = 0;
    // Resolution source plus verification state, such as "same-sector verified", "official AERA", "conflict", or "missing".
    std::string source;
};

enum class StorylandImgKind {
    None,
    LvzPair,
    ClassicDir,
    Ps2StoriesRaw,
    MobileLcsRaw,
    MobileLcsDir
};

class StorylandArchiveBrowser {
public:
    bool loadNativeScenery(const StorylandDtzArchive& dtz, StorylandNativeSceneryReport& report, std::string& errorMessage);
    bool hasNativeScenery() const { return nativeScenery; }
    void synchronizeNativeSceneryTransforms(const StorylandNativeWorld& world);
    static bool nativePlacementVisible(const StorylandWorldPlacement& placement, float distance, uint8_t area, float lodMultiplier = 1.0f);
    bool loadImgFromFile(const std::wstring& imgPath, std::string& errorMessage);
    bool loadDirWithCompanionImg(const std::wstring& dirPath, std::string& errorMessage);
    bool loadZmgFromFile(const std::wstring& zmgPath, std::string& errorMessage);
    bool loadLvzWithCompanionImg(const std::wstring& lvzPath, std::string& errorMessage,
        const StorylandVcsModloader* mods = nullptr);

    const std::vector<StorylandArchiveEntry>& entries() const;
    const std::vector<StorylandWorldPlacement>& placements() const;
    uint32_t nativeWorldMasterOffset(bool includeVcs=false) const;
    bool nativeWorldUses64BitPointers() const;
    bool nativeWorldTextureName(uint32_t resource,std::string& name) const;
    bool decodeLcsDynamicModel(uint32_t model,StorylandLcsDynamicGeometry& geometry,std::string& error) const;
    bool ensureLcsNativeDynamicInstances(const StorylandNativeWorld& world,std::string& error);
    bool setPlacementVisibility(size_t placementIndex,bool visible);
    void clearScriptPlacementVisibility() {hiddenScriptPlacements.clear();}
    bool setPlacementTransform(size_t placementIndex,const std::array<float,16>& matrix);
    const std::vector<StorylandWorldSector>& sectors() const;
    const std::vector<StorylandBuildingSwap>& buildingSwaps() const {return worldBuildingSwaps;}
    void resetBuildingSwaps();
    void setBuildingSwap(uint32_t hash,uint32_t state);
    void setBuildingSwapGroup(uint32_t group,uint32_t state);
    std::vector<bool> visibleSectors(bool scripted=true) const;
    bool hasAuthoredSectorLod() const {return compiledSectorLod;}
    bool hasNativeLcsSectorSelection() const {return lcsNativeSectorSelection;}
    std::vector<bool> visibleSectorsAt(float x,float y,bool scripted=true,int radius=0) const;
    uint32_t authoredSectorLevel() const;
    bool placementEnabled(const StorylandWorldPlacement& placement,float hour) const;
    bool worldStreamingComplete(float hour) const;
    static bool placementVisible(const StorylandWorldPlacement& placement,const std::vector<bool>& sectors);
    bool placementVisible(const StorylandWorldPlacement& placement,const std::vector<bool>& sectors,float hour) const;
    static bool placementInSector(const StorylandWorldPlacement& placement,uint32_t sector);
    const std::vector<StorylandWorldMesh>& worldMeshes() const;
    bool setPlacementModelSwap(size_t placementIndex,size_t meshIndex);
    const StorylandWorldMesh* placementModelSwap(size_t placementIndex) const;
    const std::vector<uint32_t>& masterMeshResourceIds() const;
    const std::vector<StorylandDirectTextureResource>& directTextures() const;
    const std::vector<StorylandImgResourceRow>& imgResourceRows() const;
    const std::vector<StorylandResourceResolution>& resourceResolutions() const;
    const std::wstring& imgPath() const;
    const std::vector<uint8_t>& lvzBytes() const;
    const std::vector<uint8_t>& imgBytes() const;
    const std::wstring& lvzPath() const;
    bool wasCompressedLvzInput() const { return compressedLvzInput; }
    bool hasLvzContext() const;
    bool hasImgContext() const;
    uint64_t imgFileSize() const;
    std::string levelSummary() const;
    StorylandImgKind imgKind() const;
    bool isPs2StoriesRawImg() const;
    bool isMobileLcsRawImg() const;
    bool hasClassicDirContext() const;

    bool extractEntryBytes(size_t index, std::vector<uint8_t>& outBytes, std::string& errorMessage) const;
    bool replaceEntryBytes(size_t index, const std::vector<uint8_t>& replacementBytes, std::string& report, std::string& errorMessage);
    bool addResourceBytes(const std::string& resourceName, const std::vector<uint8_t>& resourceBytes, std::string& report, std::string& errorMessage, uint32_t* addedResourceId = nullptr, const std::string& preferredTextureName = std::string());
    bool extractWorldMeshResourceBytes(uint32_t resourceId, std::vector<uint8_t>& outBytes, std::string& errorMessage) const;
    bool replaceWorldMeshResourceBytes(uint32_t resourceId, const std::vector<uint8_t>& replacementBytes, std::string& report, std::string& errorMessage);
    bool addWorldPlacement(uint32_t resourceId, float x, float y, float z, std::string& report, std::string& errorMessage);
    bool duplicateWorldPlacement(size_t placementIndex, float x, float y, float z, std::string& report, std::string& errorMessage);
    bool moveWorldPlacement(size_t placementIndex, float x, float y, float z, std::string& report, std::string& errorMessage);
    bool bindWorldMeshTextureResource(uint32_t modelResourceId, uint32_t textureResourceId, std::string& report, std::string& errorMessage);
    bool exportDirectTextureAsXtx(size_t textureIndex, std::vector<uint8_t>& outBytes, std::string& errorMessage) const;
    bool replaceDirectTextureFromArchive(size_t textureIndex, const std::vector<uint8_t>& replacementBytes, std::string& report, std::string& errorMessage);
    std::string resourceDisplayName(uint32_t resourceId) const;
    bool changeWorldMeshResourceId(uint32_t oldResourceId, uint32_t newResourceId, std::string& report, std::string& errorMessage);
    bool saveLvzImgPair(const std::wstring& lvzPath, const std::wstring& imgPath, bool compressLvz, std::string& errorMessage) const;
    bool overwriteCurrentLvzImgPair(bool compressLvz, std::string& errorMessage) const;
    bool validateLvzImgPair(std::string& report, std::string& errorMessage) const;
    bool findEntryByStemAndExtension(const std::wstring& stem, const std::initializer_list<std::wstring>& extensions, size_t& outIndex) const;
    bool findMobileLcsTextureDictionaryForEntry(size_t modelEntryIndex, size_t& outTextureEntryIndex) const;

private:
    bool nativeScenery = false;
    std::wstring currentImgPath;
    std::wstring currentLvzPath;
    uint64_t currentImgSize = 0;
    std::string currentLevelSummary;
    std::string worldPacketError;
    StorylandImgKind currentImgKind = StorylandImgKind::None;
    bool compressedLvzInput = true;
    std::vector<uint8_t> currentLvzBytes;
    std::vector<uint8_t> currentImgBytes;
    std::vector<StorylandArchiveEntry> archiveEntries;
    std::vector<StorylandWorldPlacement> worldPlacements;
    std::vector<StorylandWorldSector> worldSectors;
    bool compiledSectorLod=false;
    bool lcsNativeSectorSelection=false;
    std::vector<StorylandBuildingSwap> worldBuildingSwaps;
    bool lcsWorldLayout=false;
    struct WorldSwapCondition {uint8_t selector=0,state=0;};
    std::map<uint32_t,WorldSwapCondition> lcsPlacementConditions;
    std::map<uint32_t,WorldSwapCondition> lcsSectorConditions;
    std::set<uint64_t> decodedWorldMeshKeys;
    std::array<uint32_t,256> worldSwapGroups{};
    int32_t activeSwapSector=-1;
    std::vector<StorylandWorldMesh> worldMeshCache;
    std::map<size_t,size_t> placementModelSwaps;
    std::set<size_t> hiddenScriptPlacements;
    std::map<size_t,StorylandWorldPlacement> originalScriptPlacements;
    size_t nativeDynamicBasePlacementCount=SIZE_MAX,nativeDynamicBaseMeshCount=SIZE_MAX;
    std::vector<uint32_t> masterMeshResourceIdCache;
    std::vector<StorylandDirectTextureResource> directTextureCache;
    std::vector<StorylandImgResourceRow> imgResourceRowCache;
    std::vector<StorylandResourceResolution> resourceResolutionCache;
    std::map<uint64_t, std::string> archiveNameOverrides;
    std::map<uint32_t, std::string> masterResourceNameOverrides;
    bool lcsSwapConditionEnabled(const WorldSwapCondition& condition,float hour) const;

    bool autoFindCompanionImgForLvz(const std::wstring& lvzPath, std::wstring& outImgPath) const;
    bool autoFindCompanionLvzForImg(const std::wstring& imgPath, std::wstring& outLvzPath) const;
    bool readWholeFile(const std::wstring& path, std::vector<uint8_t>& outBytes, std::string& errorMessage) const;
    bool inflateLvzBytes(const std::vector<uint8_t>& packed, std::vector<uint8_t>& unpacked, std::string& errorMessage) const;
    bool deflateLvzBytes(const std::vector<uint8_t>& unpacked, std::vector<uint8_t>& packed, std::string& errorMessage) const;
    bool writeWholeFile(const std::wstring& path, const std::vector<uint8_t>& bytes, std::string& errorMessage) const;
    bool rebuildParsedCaches(std::string& errorMessage);
    bool buildEntriesFromLvzAndImg(std::string& errorMessage);
    bool buildEntriesFromClassicDir(const std::vector<uint8_t>& dirBytes, std::string& errorMessage);
    bool buildEntriesFromMobileLcsImg(std::string& errorMessage);
    bool buildEntriesFromRawPs2StoriesImg(std::string& errorMessage);
    void buildWorldSectorsAndPlacements();
    void buildWorldMeshes();
    void buildDirectTexturesFromLvz();
    void clear();
};
