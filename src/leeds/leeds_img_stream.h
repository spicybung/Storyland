#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// The DIR/DTZ stores sector counts. Byte offsets belong to the resolved entry below.
struct StorylandImgNamedRange {
    std::string name;
    uint32_t startSector = 0;
    uint32_t sectorCount = 0;
};

struct StorylandImgStreamEntry {
    std::string name;
    uint32_t startSector = 0;
    uint32_t sectorCount = 0;
    uint64_t byteOffset = 0;
    uint64_t byteSize = 0;
    std::string extension;
    bool hasOriginalName = false;
};

class StorylandImgStream {
public:
    bool open(const std::filesystem::path& path,
              const std::vector<StorylandImgNamedRange>& nativeEntries,
              const std::filesystem::path& companionDir,
              std::string& error);

    const std::vector<StorylandImgStreamEntry>& entries() const { return fileEntries; }
    const std::filesystem::path& sourcePath() const { return source; }
    uint64_t byteSize() const { return sourceSize; }
    const std::string& description() const { return summary; }

    bool readAt(uint64_t absoluteOffset, size_t length,
                std::vector<uint8_t>& output, std::string& error) const;
    bool readEntry(size_t index, size_t maximumBytes,
                   std::vector<uint8_t>& output, bool& partial,
                   std::string& error) const;
    bool exportEntry(size_t index, const std::filesystem::path& destination,
                     std::string& error) const;

private:
    bool loadNativeEntries(const std::vector<StorylandImgNamedRange>& nativeEntries,
                           std::string& error);
    bool loadDirectoryEntries(const std::filesystem::path& companionDir,
                              std::string& error);
    bool scanLeedsEntries(std::string& error);
    bool appendEntry(const StorylandImgNamedRange& row, bool hasName);
    std::string detectExtension(uint64_t offset) const;

    std::filesystem::path source;
    uint64_t sourceSize = 0;
    std::string summary;
    std::vector<StorylandImgStreamEntry> fileEntries;
};
