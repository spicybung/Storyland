#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

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
};


struct StorylandWorldMeshVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct StorylandWorldMeshTriangle {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t textureId = 0xFFFFFFFFu;
};

struct StorylandWorldMesh {
    uint32_t sectorIndex = 0;
    uint32_t resourceIndex = 0;
    uint64_t rawOffset = 0;
    uint32_t materialCount = 0;
    std::vector<StorylandWorldMeshVertex> vertices;
    std::vector<StorylandWorldMeshTriangle> triangles;
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
    std::string name;
    std::vector<uint8_t> rgba;
};

struct StorylandWorldSector {
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

class StorylandArchiveBrowser {
public:
    bool loadImgFromFile(const std::wstring& imgPath, std::string& errorMessage);
    bool loadLvzWithCompanionImg(const std::wstring& lvzPath, std::string& errorMessage);

    const std::vector<StorylandArchiveEntry>& entries() const;
    const std::vector<StorylandWorldPlacement>& placements() const;
    const std::vector<StorylandWorldSector>& sectors() const;
    const std::vector<StorylandWorldMesh>& worldMeshes() const;
    const std::vector<StorylandDirectTextureResource>& directTextures() const;
    const std::wstring& imgPath() const;
    const std::wstring& lvzPath() const;
    bool hasLvzContext() const;
    bool hasImgContext() const;
    uint64_t imgFileSize() const;
    std::string levelSummary() const;

    bool extractEntryBytes(size_t index, std::vector<uint8_t>& outBytes, std::string& errorMessage) const;
    bool replaceEntryBytes(size_t index, const std::vector<uint8_t>& replacementBytes, std::string& report, std::string& errorMessage);
    bool extractWorldMeshResourceBytes(uint32_t resourceId, std::vector<uint8_t>& outBytes, std::string& errorMessage) const;
    bool replaceWorldMeshResourceBytes(uint32_t resourceId, const std::vector<uint8_t>& replacementBytes, std::string& report, std::string& errorMessage);
    bool changeWorldMeshResourceId(uint32_t oldResourceId, uint32_t newResourceId, std::string& report, std::string& errorMessage);
    bool saveLvzImgPair(const std::wstring& lvzPath, const std::wstring& imgPath, bool compressLvz, std::string& errorMessage) const;
    bool overwriteCurrentLvzImgPair(bool compressLvz, std::string& errorMessage) const;
    bool findEntryByStemAndExtension(const std::wstring& stem, const std::initializer_list<std::wstring>& extensions, size_t& outIndex) const;

private:
    std::wstring currentImgPath;
    std::wstring currentLvzPath;
    uint64_t currentImgSize = 0;
    std::string currentLevelSummary;
    std::vector<uint8_t> currentLvzBytes;
    std::vector<uint8_t> currentImgBytes;
    std::vector<StorylandArchiveEntry> archiveEntries;
    std::vector<StorylandWorldPlacement> worldPlacements;
    std::vector<StorylandWorldSector> worldSectors;
    std::vector<StorylandWorldMesh> worldMeshCache;
    std::vector<StorylandDirectTextureResource> directTextureCache;

    bool autoFindCompanionImgForLvz(const std::wstring& lvzPath, std::wstring& outImgPath) const;
    bool autoFindCompanionLvzForImg(const std::wstring& imgPath, std::wstring& outLvzPath) const;
    bool readWholeFile(const std::wstring& path, std::vector<uint8_t>& outBytes, std::string& errorMessage) const;
    bool inflateLvzBytes(const std::vector<uint8_t>& packed, std::vector<uint8_t>& unpacked, std::string& errorMessage) const;
    bool deflateLvzBytes(const std::vector<uint8_t>& unpacked, std::vector<uint8_t>& packed, std::string& errorMessage) const;
    bool writeWholeFile(const std::wstring& path, const std::vector<uint8_t>& bytes, std::string& errorMessage) const;
    bool rebuildParsedCaches(std::string& errorMessage);
    bool buildEntriesFromLvzAndImg(std::string& errorMessage);
    void buildWorldSectorsAndPlacements();
    void buildWorldMeshes();
    void buildDirectTexturesFromLvz();
    void clear();
};
