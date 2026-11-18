#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace storyland_ctw {

struct WadResource {
    uint32_t hash = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
    std::string fileType;
};

class WadArchive {
public:
    bool loadFromFile(const std::filesystem::path& file, std::string& error);
    bool extractResource(size_t index, std::vector<uint8_t>& bytes, std::string& error) const;
    bool exportResource(size_t index, const std::filesystem::path& file, std::string& error) const;
    void clear();

    const std::filesystem::path& wadPath() const { return wadPath_; }
    const std::filesystem::path& tocPath() const { return tocPath_; }
    const std::vector<WadResource>& resources() const { return resources_; }
    uint64_t fileSize() const { return fileSize_; }

private:
    std::filesystem::path wadPath_;
    std::filesystem::path tocPath_;
    std::vector<WadResource> resources_;
    uint64_t fileSize_ = 0;
};

}
