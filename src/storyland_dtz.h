#pragma once
#include <cstdint>
#include <string>
#include <vector>

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
    uint32_t startSector = 0;
    uint32_t sectorCount = 0;
    uint32_t matchedDtzSectorCount = 0;
    bool countDiffersFromCompanionDir = false;
    std::string name;
    std::vector<size_t> matchingRecordIndices;
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
    bool loadFromFile(const std::wstring& filePath, std::string& errorMessage);
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
    bool extractDirEntryBytes(size_t dirEntryIndex, std::vector<uint8_t>& bytes, std::string& errorMessage) const;
    bool findDirEntryByStemAndExtension(const std::wstring& stem, const std::vector<std::wstring>& extensions, size_t& outIndex) const;

    const std::vector<StorylandDtzHeaderField>& headerFields() const;
    const std::vector<StorylandDtzSectorRecord>& sectorRecords() const;
    const std::vector<StorylandDtzResourceHint>& resourceHints() const;
    const std::vector<StorylandDtzDirEntry>& dirEntries() const;
    const std::vector<StorylandDtzDataBlock>& dataBlocks() const;
    const std::vector<StorylandDtzDataField>& dataFields() const;
    const std::vector<uint8_t>& unpackedBytes() const;
    const std::wstring& sourcePath() const;
    const std::wstring& companionDirPath() const;
    const std::wstring& companionImgPath() const;
    uint64_t companionImgSize() const;
    bool wasCompressedInput() const;
    bool hasCompanionDir() const;
    bool hasCompanionImg() const;

private:
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
    std::vector<uint8_t> dirRawData;
    std::vector<uint8_t> imgRawData;
    bool compressedInput = false;

    bool writePatchedCompanionDir(const std::wstring& savedDtzPath, std::string& errorMessage) const;
    bool writePatchedCompanionImg(const std::wstring& savedDtzPath, std::string& errorMessage) const;
    void patchCompanionDirMap(uint32_t selectedStartSector, uint32_t selectedOldSectorCount, uint32_t selectedNewSectorCount, bool shiftLaterStarts);
    bool resizeCompanionImgForSectorPatch(uint32_t selectedStartSector, uint32_t selectedOldSectorCount, uint32_t selectedNewSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage);

    bool parse(std::string& errorMessage);
    void rebuildSectorRecords();
    void rebuildDirMatches();
    void rebuildDataBlocksAndFields();
    void tryAutoLoadCompanionDir();
    void tryAutoLoadCompanionImg();
};
