#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct StorylandScmHeader {
    bool valid = false;
    char targetGame = 0;
    uint32_t mainScriptSize = 0;
    uint32_t largestMissionScriptSize = 0;
    uint32_t mainScriptOffset = 0;
    uint32_t globalVariableBytes = 0;
    uint32_t saveVariableCount = 0;
    uint32_t objectCount = 0;
    uint16_t trueGlobalCount = 0;
    uint16_t mostGlobalCount = 0;
    uint16_t missionCount = 0;
    uint16_t exclusiveMissionCount = 0;
    uint32_t missionTableOffset = 0;
};

struct StorylandScmMission {
    int id = -1;
    std::string label;
    std::string name;
    size_t sourceBegin = 0;
    size_t sourceEnd = 0;
    std::string source;
    uint32_t binaryOffset = 0;
    uint32_t binarySize = 0;
    bool hasBinaryRange = false;
};

class StorylandScmFile {
public:
    bool loadFromFile(const std::wstring& filePath, std::string& errorMessage);
    bool setDecompiledSource(const std::string& text, const std::string& mode, std::string& errorMessage);
    bool replaceMissionSource(size_t missionIndex, const std::string& text, std::string& errorMessage);
    bool applyMissionNamesFile(const std::wstring& filePath, std::string& errorMessage);

    const std::wstring& sourcePath() const;
    const std::vector<uint8_t>& rawBytes() const;
    const std::string& sourceText() const;
    const std::string& modeId() const;
    const StorylandScmHeader& header() const;
    const std::vector<StorylandScmMission>& missions() const;

    bool hasNativeMissionTable() const;
    bool hasDecompiledSource() const;
    std::string missionSource(size_t missionIndex) const;
    std::vector<uint8_t> missionBinary(size_t missionIndex) const;
    std::string summaryLine() const;

private:
    std::wstring path;
    std::vector<uint8_t> data;
    std::string decompiledSource;
    std::string activeMode;
    StorylandScmHeader nativeHeader;
    std::vector<StorylandScmMission> nativeMissionRows;
    std::vector<StorylandScmMission> missionRows;

    void parseNativeHeader();
    void parseMissions();
};
