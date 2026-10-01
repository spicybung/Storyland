#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct StorylandWblBox {
    uint32_t index = 0;
    uint32_t offset = 0;
    uint32_t tag = 0;
    int16_t rawMinX = 0;
    int16_t rawMinY = 0;
    int16_t rawMinZ = 0;
    int16_t rawMaxX = 0;
    int16_t rawMaxY = 0;
    int16_t rawMaxZ = 0;
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
    bool visible = false;
};

struct StorylandWblSection {
    uint32_t index = 0;
    uint32_t offset = 0;
    uint32_t count = 0;
    uint32_t stride = 0;
    uint32_t byteSize = 0;
    std::string name;
};

struct StorylandWblVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct StorylandWblTriangle {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint16_t textureId = 0;
};

struct StorylandWblMesh {
    uint32_t index = 0;
    uint32_t offset = 0;
    uint32_t resourceId = 0;
    uint32_t firstVertex = 0;
    uint32_t vertexCount = 0;
    uint32_t firstTriangle = 0;
    uint32_t triangleCount = 0;
    uint8_t materialCount = 0;
    float scaleFactor = 1.0f;
};

class StorylandWblFile {
public:
    bool loadFromFile(const std::wstring& path, std::string& errorMessage);
    void clear();

    const std::wstring& sourcePath() const;
    const std::vector<uint8_t>& bytes() const;
    const std::vector<StorylandWblBox>& boxes() const;
    const std::vector<StorylandWblSection>& sections() const;
    const std::vector<StorylandWblVertex>& vertices() const;
    const std::vector<StorylandWblTriangle>& triangles() const;
    const std::vector<StorylandWblMesh>& meshes() const;
    const std::vector<std::string>& lines() const;

    uint64_t fileSize() const;
    uint32_t pageCount() const;
    int32_t originXRaw() const;
    int32_t originYRaw() const;
    float originX() const;
    float originY() const;
    uint16_t primaryCount() const;
    uint16_t secondaryCount() const;
    uint16_t tertiaryCount() const;
    uint16_t tableA() const;
    uint16_t tableB() const;
    uint16_t tableC() const;
    uint16_t tableD() const;
    uint16_t tableE() const;
    bool hasBounds(float& minX, float& minY, float& minZ, float& maxX, float& maxY, float& maxZ) const;

private:
    std::wstring currentPath;
    std::vector<uint8_t> rawBytes;
    std::vector<StorylandWblBox> parsedBoxes;
    std::vector<StorylandWblSection> parsedSections;
    std::vector<StorylandWblVertex> parsedVertices;
    std::vector<StorylandWblTriangle> parsedTriangles;
    std::vector<StorylandWblMesh> parsedMeshes;
    std::vector<std::string> parsedLines;
    int32_t headerOriginXRaw = 0;
    int32_t headerOriginYRaw = 0;
    uint16_t headerPrimaryCount = 0;
    uint16_t headerSecondaryCount = 0;
    uint16_t headerTertiaryCount = 0;
    uint16_t headerTableA = 0;
    uint16_t headerTableB = 0;
    uint16_t headerTableC = 0;
    uint16_t headerTableD = 0;
    uint16_t headerTableE = 0;

    void parse();
};
