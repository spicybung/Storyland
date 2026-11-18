#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace storyland_ctw {

struct PakHeader {
    uint32_t version = 0;
    bool mobile = false;
    uint32_t sectorSize = 2048;
    std::array<uint32_t, 8> bankStarts{};
    std::vector<uint64_t> offsets;
    uint32_t entryCount = 0;
    uint32_t dataOffset = 0;
    std::vector<uint16_t> resourceIds;
};

struct PakResource {
    uint64_t offset = 0;
    uint32_t storedSize = 0;
    bool compressed = false;
    std::string type;
    std::string name;
    uint32_t compressedSize = 0;
    uint32_t expandedSize = 0;
    uint16_t resourceId = 0;
    bool hasResourceId = false;
    bool isPspExecutable = false;
};

bool isArchiveSource(const std::filesystem::path& source, const std::filesystem::path& destination);

uint16_t read16(const unsigned char* data);
uint32_t read32(const unsigned char* data);

bool readPakIndex(const std::filesystem::path& path, PakHeader& header, std::string& error);
bool readPakIndex(const std::string& filename, PakHeader& header, std::string& error);

class PakArchive {
public:
    bool loadFromFile(const std::filesystem::path& path, std::string& error);
    bool extractResource(size_t index, std::vector<uint8_t>& bytes, std::string& error) const;
    bool exportResource(size_t index, const std::filesystem::path& path, std::string& error) const;
    void clear();

    const std::filesystem::path& sourcePath() const { return path_; }
    const PakHeader& header() const { return header_; }
    const std::vector<PakResource>& resources() const { return resources_; }
    bool isOverlayArchive() const { return overlays_; }
    uint64_t fileSize() const { return fileSize_; }

private:
    std::filesystem::path path_;
    PakHeader header_;
    std::vector<PakResource> resources_;
    uint64_t fileSize_ = 0;
    bool overlays_ = false;
};

struct PakPlacement {
    uint16_t instanceId = 0;
    uint16_t modelId = 0;
    uint16_t angle = 0;
    float x = 0;
    float y = 0;
    float z = 0;
    uint16_t flags = 0;
    int16_t rawX = 0;
    int16_t rawY = 0;
    int16_t rawZ = 0;
};

struct PakWorldSection {
    std::vector<PakPlacement> placements;
    uint32_t groupCount = 0;
    uint16_t shadowCount = 0;
    uint32_t lightCount = 0;
    bool boundaryOnly = false;
};

bool decodePspWorldSection(const std::vector<uint8_t>& bytes, unsigned section,
                           PakWorldSection& decoded, std::string& error);
bool decodePspWorldSection(const std::vector<uint8_t>& bytes, unsigned section,
                           std::vector<PakPlacement>& placements, std::string& error);
struct PakVertex {
    float x, y, z, u, v;
    float nx = 0;
    float ny = 0;
    float nz = 0;
};
struct PakModelPart {
    uint32_t firstVertex = 0;
    uint32_t vertexCount = 0;
    uint16_t textureId = 0;
    uint8_t flags = 0;
    uint8_t alpha = 255;
};
struct PakModel { std::vector<PakVertex> vertices; std::vector<PakModelPart> parts; };
bool decodePspModel(const std::vector<uint8_t>& bytes, PakModel& model, std::string& error);
bool decodePspTexture(const std::vector<uint8_t>& bytes, uint32_t& width, uint32_t& height,
                      std::vector<uint8_t>& rgba, std::string& error);
std::string guessResourceExtension(const std::vector<uint8_t>& bytes);

}
