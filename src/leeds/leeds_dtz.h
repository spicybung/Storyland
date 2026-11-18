#pragma once
#include "src/leeds/leeds_native_world.h"
#include "src/re/reVCS/modloader/storyland_vcs_modloader.h"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

enum class StorylandDtzGameVariant { Unknown, Lcs, Vcs };

struct StorylandDtzHeaderField {
    std::string name;
    uint32_t offset = 0;
    uint32_t value = 0;
    std::string note;
};

struct StorylandDtzSectorRecord {
    uint32_t recordOffset = 0;
    uint32_t startOffset = 0;
    uint32_t countOffset = 0;
    uint32_t startSector = 0;
    uint32_t sectorCount = 0;
    std::string source;
    std::string resourceName;
    std::string note;
};

struct StorylandDtzResourceHint {
    std::string name;
    uint32_t offset = 0;
    uint32_t size = 0;
    std::string note;
};

struct StorylandDtzDirEntry {
    uint32_t dirIndex = 0;
    uint32_t streamingIndex = 0xFFFFFFFFu;
    uint32_t startSector = 0;
    uint32_t sectorCount = 0;
    uint32_t matchedDtzSectorCount = 0;
    uint64_t byteOffset = 0;
    uint64_t byteLength = 0;
    uint64_t availableBytes = 0;
    bool countDiffersFromCompanionDir = false;
    bool fullyBackedByImg = false;
    // Native table type and physical read range are independent of the saved
    // DTZ allocation. A verified companion recovery never rewrites that table.
    bool imageRangeMismatch = false;
    std::string expectedExtension;
    std::string detectedExtension;
    std::string name;
    uint32_t startStorageOffset = 0xFFFFFFFFu;
    uint32_t countStorageOffset = 0xFFFFFFFFu;
    uint32_t nameStorageOffset = 0xFFFFFFFFu;
    uint32_t nameStorageLength = 0u;
    bool nameStoredAsHash = false;
    bool nameStoredAsSplitExtension = false;
    std::vector<size_t> matchingRecordIndices;
};


struct StorylandDtz2dfxEffect {
    uint32_t index = 0;
    uint32_t rowOffset = 0;
    float localX = 0.0f;
    float localY = 0.0f;
    float localZ = 0.0f;
    float positionW = 0.0f;
    uint8_t red = 255;
    uint8_t green = 255;
    uint8_t blue = 255;
    uint8_t alpha = 255;
    uint8_t effectType = 0;

    // Corona draw distance and lamp range are separate values in the DTZ.
    float coronaFarClip = 0.0f;
    float pointLightRange = 0.0f;
    float coronaSize = 0.0f;
    float shadowSize = 0.0f;
    uint8_t lightType = 0;
    uint8_t roadReflection = 0;
    uint8_t flareType = 0;
    uint8_t shadowIntensity = 0;
    uint8_t flags = 0;
    uint32_t coronaTexturePointer = 0;
    uint32_t shadowTexturePointer = 0;
    std::string coronaTextureName;
    std::string shadowTextureName;

    int32_t particleSubtype = 0;
    float directionX = 0.0f;
    float directionY = 0.0f;
    float directionZ = 0.0f;
    float particleScale = 0.0f;
    uint8_t attractorSubtype = 0;
    uint8_t attractorProbability = 0;
    float pedRotationX = 0.0f;
    float pedRotationY = 0.0f;
    float pedRotationZ = 0.0f;
    uint8_t pedSubtype = 0;

    int32_t modelIndex = -1;
    uint32_t modelHash = 0;
    uint32_t modelInfoOffset = 0;
    uint8_t modelType = 0;
    uint32_t modelEffectIndex = 0;
    std::string modelName;
    bool hasModelAssociation = false;
    bool isLight = false;
    bool valid = false;
};

struct StorylandDtz2dfxWorldInstance {
    uint32_t index = 0;
    uint32_t effectIndex = 0;
    uint32_t entityOffset = 0;
    uint32_t poolIndex = 0;
    std::string poolName;
    uint8_t poolFlag = 0;
    int32_t modelIndex = -1;
    int32_t secondaryModelIndex = -1;
    uint8_t level = 0;
    uint8_t area = 0;
    uint32_t poolHeader = 0;
    uint16_t lightPhase = 0;
    bool nativeProcessLights = true;

    float rightX = 1.0f;
    float rightY = 0.0f;
    float rightZ = 0.0f;
    float upX = 0.0f;
    float upY = 1.0f;
    float upZ = 0.0f;
    float atX = 0.0f;
    float atY = 0.0f;
    float atZ = 1.0f;
    float entityX = 0.0f;
    float entityY = 0.0f;
    float entityZ = 0.0f;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    bool valid = false;
};

struct StorylandDtzDataBlock {
    std::string name;
    std::string parser;
    uint32_t headerOffset = 0;
    uint32_t offset = 0;
    uint32_t inferredEnd = 0;
    uint32_t size = 0;
    uint32_t rowSize = 0;
    uint32_t rowCount = 0;
    bool editable = false;
    std::string note;
};

struct StorylandDtzDataField {
    bool hasLimits = false;
    double minimum = 0, maximum = 0;
    size_t blockIndex = 0;
    uint32_t rowIndex = 0;
    uint32_t absoluteOffset = 0;
    uint32_t relativeOffset = 0;
    uint32_t size = 0;
    std::string blockName;
    std::string rowLabel;
    std::string name;
    std::string type;
    std::string valueText;
    std::string note;
    bool editable = false;
};

class StorylandDtzArchive {
public:
    bool extractNativeAnimationBank(const std::vector<std::string>& names,std::vector<uint8_t>& bytes,std::string& error) const;

    bool loadFromFile(const std::wstring& filePath, std::string& errorMessage, bool loadCompanion = true);
    bool loadCompanionDir(const std::wstring& filePath, std::string& errorMessage);
    bool loadCompanionImg(const std::wstring& filePath, std::string& errorMessage);
    bool saveToFile(const std::wstring& filePath, bool compressOutput, std::string& errorMessage) const;
    bool saveCompanionImgToFile(const std::wstring& filePath, std::string& errorMessage) const;

    bool patchSectorRecord(size_t recordIndex, uint32_t newSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage);
    bool patchDirEntry(size_t dirEntryIndex, uint32_t newSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage);
    bool patchExactSectorPair(uint32_t oldStartSector, uint32_t oldSectorCount, uint32_t newSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage);
    bool patchDataField(size_t fieldIndex, const std::string& newValueText, std::string& report, std::string& errorMessage);
    bool patchRawBytes(uint32_t absoluteOffset, const std::vector<uint8_t>& bytes, std::string& report, std::string& errorMessage);
    bool replaceDirEntryBytes(size_t dirEntryIndex, const std::vector<uint8_t>& replacementBytes, bool shiftLaterStarts, std::string& report, std::string& errorMessage);
    bool renameDirEntry(size_t dirEntryIndex, const std::string& requestedName, std::string& report, std::string& errorMessage);
    bool extractDirEntryBytes(size_t dirEntryIndex, std::vector<uint8_t>& bytes, std::string& errorMessage) const;
    bool findModelDirEntryByModelId(uint32_t modelId, size_t& outIndex, std::string& outName) const;
    bool findModelIdByName(const std::string& name, uint32_t& modelId) const;
    bool findDirEntryByStemAndExtension(const std::wstring& stem, const std::vector<std::wstring>& extensions, size_t& outIndex) const;

    const std::vector<StorylandDtzHeaderField>& headerFields() const;
    const std::vector<StorylandDtzSectorRecord>& sectorRecords() const;
    const std::vector<StorylandDtzResourceHint>& resourceHints() const;
    const std::vector<StorylandDtzDirEntry>& dirEntries() const;
    const std::vector<StorylandDtzDataBlock>& dataBlocks() const;
    const std::vector<StorylandDtzDataField>& dataFields() const;
    const std::vector<StorylandDtz2dfxEffect>& leeds2dfxEffects() const;
    const std::vector<StorylandDtz2dfxWorldInstance>& leeds2dfxWorldInstances() const;
    const std::vector<uint8_t>& unpackedBytes() const;
    StorylandNativeWorld& nativeWorld();
    const StorylandNativeWorld& nativeWorld() const;
    void bindScriptCollisionWorld(const StorylandMissionEnvironment* environment);
    bool setNativeInstanceMatrix(size_t index,const std::array<float,16>& matrix);
    bool ensureNativeCollision(const StorylandNativePoint& point,std::string& error);
    void resetBuildingSwaps();
    bool replaceBuildingModel(uint64_t worldKey,uint32_t model);
    bool setNativeInstanceTransform(uint64_t worldKey,const std::array<float,16>& matrix);
    void setFortStauntonDestroyed(bool destroyed);
    bool requestCollision(const StorylandNativePoint& position,int32_t fallbackLevel,std::string& error);
    bool loadSceneCollision(const StorylandNativePoint& position,int32_t fallbackLevel,
        const std::array<int32_t,2>& allowedLevels,bool enabled,std::string& error);
    void setCollisionLevels(int32_t level,const std::array<int32_t,2>& allowedLevels,bool enabled);
    void resetCollisionStreaming();
    size_t applyBuildingSwaps(const std::vector<StorylandBuildingSwap>& swaps,
        const std::map<uint32_t,int32_t>& states);
    const std::wstring& sourcePath() const;
    const std::wstring& companionDirPath() const;
    const std::wstring& companionImgPath() const;
    uint64_t companionImgSize() const;
    const std::string& companionStatus() const { return companionImageStatus; }
    uint64_t contentRevision() const {return contentGeneration;}
    const StorylandVcsModloader* modloader() const { return mountedMods.get(); }
    std::filesystem::path resolveGameFile(const std::string& retailName,
        const std::filesystem::path& fallback = {}) const;
    bool wasCompressedInput() const;
    bool hasCompanionDir() const;
    bool hasCompanionImg() const;
    StorylandDtzGameVariant gameVariant() const;
    bool isPsp() const { return nativePsp; }
    size_t unbackedStreamCount() const;

private:
    struct OriginalUnbackedRange {
        uint32_t streamingIndex, startOffset, countOffset, count;
        int64_t relativeToImgEnd;
    };
    std::vector<OriginalUnbackedRange> originalUnbackedRanges;
    void rememberOriginalUnbackedRanges();
    StorylandNativeWorld nativeWorldData;
    std::shared_ptr<StorylandVcsModloader> mountedMods;
    void rebuildNativeWorld();
    std::wstring path;
    std::wstring dirPath;
    std::wstring imgPath;
    std::vector<uint8_t> unpackedData;
    std::vector<StorylandDtzHeaderField> headers;
    std::vector<StorylandDtzSectorRecord> records;
    std::vector<StorylandDtzResourceHint> hints;
    std::vector<StorylandDtzDirEntry> dirMap;
    std::vector<StorylandDtzDirEntry> externalDirMap;
    std::vector<StorylandDtzDataBlock> dataBlocksCache;
    std::vector<StorylandDtzDataField> dataFieldsCache;
    std::vector<StorylandDtz2dfxEffect> leeds2dfxEffectsCache;
    std::vector<StorylandDtz2dfxWorldInstance> leeds2dfxWorldInstancesCache;
    std::vector<uint8_t> dirRawData;
    std::vector<uint8_t> imgRawData;
    std::string companionImageStatus;
    bool companionImageMismatch = false;
    bool compressedInput = false;
    StorylandDtzGameVariant detectedGameVariant = StorylandDtzGameVariant::Unknown;
    bool nativePsp = false;
    std::map<uint32_t, std::string> explicitStreamNames;

    bool writePatchedCompanionDir(const std::wstring& savedDtzPath, std::string& errorMessage) const;
    bool writePatchedCompanionImg(const std::wstring& savedDtzPath, std::string& errorMessage) const;
    void patchCompanionDirMap(uint32_t selectedStartSector, uint32_t selectedOldSectorCount, uint32_t selectedNewSectorCount, bool shiftLaterStarts);
    bool resizeCompanionImgForSectorPatch(uint32_t selectedStartSector, uint32_t selectedOldSectorCount, uint32_t selectedNewSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage);

    bool parse(std::string& errorMessage, bool rebuildDirectory = true);
    void rebuildSectorRecords();
    void rebuildDirMatches(bool initialLoad = false);
    void validateCompanionImageRanges();
    void rebuildDataBlocksAndFields();
    void rebuildLeeds2dfxEffects();
    void rebuildLeeds2dfxWorldInstances();
    void tryAutoLoadCompanionImg();
    uint64_t contentGeneration=0;
};
