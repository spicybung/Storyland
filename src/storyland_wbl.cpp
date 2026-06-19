#include "storyland_wbl.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

static uint16_t wblReadU16(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 2 > bytes.size()) return 0;
    return uint16_t(bytes[offset] | (uint16_t(bytes[offset + 1]) << 8));
}

static int16_t wblReadI16(const std::vector<uint8_t>& bytes, size_t offset) {
    return int16_t(wblReadU16(bytes, offset));
}

static uint32_t wblReadU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0;
    return uint32_t(bytes[offset]) |
           (uint32_t(bytes[offset + 1]) << 8) |
           (uint32_t(bytes[offset + 2]) << 16) |
           (uint32_t(bytes[offset + 3]) << 24);
}

static int32_t wblReadI32(const std::vector<uint8_t>& bytes, size_t offset) {
    return int32_t(wblReadU32(bytes, offset));
}

static int8_t wblReadI8(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset >= bytes.size()) return 0;
    return int8_t(bytes[offset]);
}

static float wblReadF32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0.0f;
    uint32_t raw = wblReadU32(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

static bool wblRangeFits(const std::vector<uint8_t>& bytes, uint32_t offset, uint32_t size) {
    return uint64_t(offset) + uint64_t(size) <= uint64_t(bytes.size());
}

static bool wblReadWholeFile(const std::wstring& path, std::vector<uint8_t>& outBytes, std::string& errorMessage) {
    outBytes.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        errorMessage = "Could not open file.";
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size < 0) {
        errorMessage = "Could not get file size.";
        return false;
    }
    file.seekg(0, std::ios::beg);

    outBytes.resize(size_t(size));
    if (!outBytes.empty()) file.read(reinterpret_cast<char*>(outBytes.data()), std::streamsize(outBytes.size()));
    if (!file && !outBytes.empty()) {
        errorMessage = "Could not read full file.";
        outBytes.clear();
        return false;
    }
    return true;
}

static std::string wblHex(uint32_t value, int width = 8) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "0x%0*X", width, value);
    return buffer;
}

void StorylandWblFile::clear() {
    currentPath.clear();
    rawBytes.clear();
    parsedBoxes.clear();
    parsedSections.clear();
    parsedVertices.clear();
    parsedTriangles.clear();
    parsedMeshes.clear();
    parsedLines.clear();
    headerOriginXRaw = 0;
    headerOriginYRaw = 0;
    headerPrimaryCount = 0;
    headerSecondaryCount = 0;
    headerTertiaryCount = 0;
    headerTableA = 0;
    headerTableB = 0;
    headerTableC = 0;
    headerTableD = 0;
    headerTableE = 0;
}

bool StorylandWblFile::loadFromFile(const std::wstring& path, std::string& errorMessage) {
    clear();
    if (!wblReadWholeFile(path, rawBytes, errorMessage)) return false;
    if (rawBytes.size() < 0x30) {
        errorMessage = "WBL file is too small.";
        clear();
        return false;
    }

    uint32_t page0 = wblReadU32(rawBytes, 0x00);
    uint32_t page1 = wblReadU32(rawBytes, 0x08);
    uint32_t page2 = wblReadU32(rawBytes, 0x10);
    if (page0 != 0x1000u || page1 != 0x1000u || page2 != 0x1000u) {
        errorMessage = "This does not look like a Chinatown Wars .wbl worldblock header.";
        clear();
        return false;
    }

    currentPath = path;
    parse();
    return true;
}

void StorylandWblFile::parse() {
    parsedBoxes.clear();
    parsedSections.clear();
    parsedVertices.clear();
    parsedTriangles.clear();
    parsedMeshes.clear();
    parsedLines.clear();

    headerOriginXRaw = wblReadI32(rawBytes, 0x14);
    headerOriginYRaw = wblReadI32(rawBytes, 0x18);

    float headerPosX = float(headerOriginXRaw) / 4096.0f;
    float headerPosY = float(headerOriginYRaw) / 4096.0f;
    float headerPosZ = wblReadI32(rawBytes, 0x1C) / 4096.0f;

    struct SectorInfo {
        uint32_t offset = 0;
        int16_t instances = 0;
        int16_t shadows = 0;
        int16_t levels = 0;
        int16_t lights = 0;
        int16_t textures = 0;
    };

    std::vector<SectorInfo> sectors;
    std::set<uint32_t> meshOffsets;
    std::map<uint32_t, uint32_t> meshResourceIds;

    uint32_t sectorOffset = 0x28u;
    for (uint32_t sectorIndex = 0; sectorIndex < 4; ++sectorIndex) {
        if (!wblRangeFits(rawBytes, sectorOffset, 12)) break;

        SectorInfo sector;
        sector.offset = sectorOffset;
        sector.instances = int16_t(wblReadI16(rawBytes, sectorOffset + 2));
        sector.shadows = int16_t(wblReadI16(rawBytes, sectorOffset + 4));
        sector.levels = int16_t(wblReadI16(rawBytes, sectorOffset + 6));
        sector.lights = int16_t(wblReadI16(rawBytes, sectorOffset + 8));
        sector.textures = int16_t(wblReadI16(rawBytes, sectorOffset + 10));

        if (sector.instances < 0 || sector.shadows < 0 || sector.levels < 0 || sector.lights < 0 || sector.textures < 0) break;
        if (sector.instances > 20000 || sector.shadows > 20000 || sector.levels > 20000 || sector.lights > 20000 || sector.textures > 20000) break;

        uint64_t levelOffset = uint64_t(sectorOffset) + 12ull;
        uint64_t instanceOffset = levelOffset + uint64_t(sector.levels) * 16ull;
        uint64_t afterInstances = instanceOffset + uint64_t(sector.instances) * 16ull;
        uint64_t afterShadows = afterInstances + uint64_t(sector.shadows) * 20ull;
        uint64_t afterLights = afterShadows + uint64_t(sector.lights) * 20ull;
        uint64_t afterTextures = afterLights + uint64_t(sector.textures) * 2ull;
        if (afterTextures > rawBytes.size() || afterTextures > 0xFFFFFFFFull) break;

        for (int i = 0; i < sector.instances; ++i) {
            uint32_t inst = uint32_t(instanceOffset + uint64_t(i) * 16ull);
            uint32_t resourceId = wblReadU32(rawBytes, inst + 4);
            uint32_t meshOffset = wblReadU32(rawBytes, inst + 8);
            if (meshOffset != 0 && wblRangeFits(rawBytes, meshOffset, 48)) {
                meshOffsets.insert(meshOffset);
                if (meshResourceIds.find(meshOffset) == meshResourceIds.end()) meshResourceIds[meshOffset] = resourceId;
            }
        }

        sectors.push_back(sector);
        sectorOffset = uint32_t(afterTextures);
    }

    headerPrimaryCount = uint16_t(std::min<size_t>(meshOffsets.size(), 0xFFFFu));
    headerSecondaryCount = 0;
    headerTertiaryCount = 0;
    headerTableA = sectors.size() > 0 ? uint16_t(sectors[0].instances) : 0;
    headerTableB = sectors.size() > 1 ? uint16_t(sectors[1].instances) : 0;
    headerTableC = sectors.size() > 2 ? uint16_t(sectors[2].instances) : 0;
    headerTableD = sectors.size() > 3 ? uint16_t(sectors[3].instances) : 0;
    headerTableE = 0;

    for (size_t i = 0; i < sectors.size(); ++i) {
        StorylandWblSection section;
        section.index = uint32_t(i);
        section.offset = sectors[i].offset;
        section.count = uint32_t(sectors[i].instances);
        section.stride = 16;
        section.byteSize = 12u + uint32_t(sectors[i].levels) * 16u + uint32_t(sectors[i].instances) * 16u + uint32_t(sectors[i].shadows) * 20u + uint32_t(sectors[i].lights) * 20u + uint32_t(sectors[i].textures) * 2u;
        std::ostringstream name;
        name << "sector " << i << " instances=" << sectors[i].instances << " levels=" << sectors[i].levels << " shadows=" << sectors[i].shadows << " lights=" << sectors[i].lights << " textures=" << sectors[i].textures;
        section.name = name.str();
        parsedSections.push_back(section);
    }

    auto addTriStrip = [&](uint32_t baseVertex, uint32_t count, uint16_t textureId) {
        if (count < 3) return;
        bool flip = false;
        for (uint32_t i = 0; i + 2 < count; ++i) {
            uint32_t a = baseVertex + i;
            uint32_t b = baseVertex + i + 1;
            uint32_t c = baseVertex + i + 2;
            if (flip) std::swap(b, c);
            flip = !flip;
            if (a == b || b == c || a == c) continue;
            StorylandWblTriangle tri;
            tri.a = a;
            tri.b = b;
            tri.c = c;
            tri.textureId = textureId;
            parsedTriangles.push_back(tri);
        }
    };

    uint32_t meshIndex = 0;
    for (uint32_t meshOffset : meshOffsets) {
        if (!wblRangeFits(rawBytes, meshOffset, 48)) continue;

        int8_t materialCountRaw = wblReadI8(rawBytes, meshOffset + 5);
        int16_t vertexCountRaw = wblReadI16(rawBytes, meshOffset + 6);
        if (materialCountRaw <= 0 || materialCountRaw > 64) continue;
        if (vertexCountRaw <= 0 || vertexCountRaw > 6550) continue;

        uint32_t vertexCount = uint32_t(vertexCountRaw);
        uint32_t materialCount = uint32_t(materialCountRaw);
        float uvOffset = wblReadF32(rawBytes, meshOffset + 40);
        float scaleFactor = wblReadF32(rawBytes, meshOffset + 44);
        if (!std::isfinite(scaleFactor) || std::abs(scaleFactor) < 0.000001f) continue;

        uint32_t vertexBase = meshOffset + 48u;
        uint64_t materialTable = uint64_t(vertexBase) + uint64_t(vertexCount) * 16ull;
        uint64_t meshEnd = materialTable + uint64_t(materialCount) * 12ull;
        if (meshEnd > rawBytes.size()) continue;

        uint32_t firstVertex = uint32_t(parsedVertices.size());
        uint32_t firstTriangle = uint32_t(parsedTriangles.size());

        for (uint32_t v = 0; v < vertexCount; ++v) {
            uint32_t off = vertexBase + v * 16u;
            int16_t rx = wblReadI16(rawBytes, off + 0);
            int16_t ry = wblReadI16(rawBytes, off + 2);
            int16_t rz = wblReadI16(rawBytes, off + 4);
            int16_t ru = wblReadI16(rawBytes, off + 12);
            int16_t rv = wblReadI16(rawBytes, off + 14);

            StorylandWblVertex vert;
            vert.x = float(rx) / scaleFactor + headerPosX;
            vert.y = float(ry) / scaleFactor + headerPosY;
            vert.z = float(rz) / scaleFactor + headerPosZ;
            vert.u = float(ru) / 2048.0f + uvOffset * 2.0f;
            vert.v = 1.0f - float(rv) / 2048.0f;
            parsedVertices.push_back(vert);
        }

        uint32_t partStart = 0;
        for (uint32_t m = 0; m < materialCount; ++m) {
            uint32_t moff = uint32_t(materialTable) + m * 12u;
            uint16_t textureId = wblReadU16(rawBytes, moff + 0);
            uint16_t partVertexCount = wblReadU16(rawBytes, moff + 2);
            uint8_t renderFlags = uint8_t(rawBytes[moff + 4]);
            float uvMul = renderFlags == 4 ? 3.0f : 2.0f;
            if (partStart + uint32_t(partVertexCount) > vertexCount) break;
            for (uint32_t i = 0; i < partVertexCount; ++i) {
                StorylandWblVertex& vert = parsedVertices[size_t(firstVertex + partStart + i)];
                vert.u += uvOffset * uvMul;
            }
            addTriStrip(firstVertex + partStart, uint32_t(partVertexCount), textureId);
            partStart += uint32_t(partVertexCount);
        }

        uint32_t triangleCount = uint32_t(parsedTriangles.size()) - firstTriangle;
        if (triangleCount == 0) {
            parsedVertices.resize(firstVertex);
            parsedTriangles.resize(firstTriangle);
            continue;
        }

        StorylandWblMesh mesh;
        mesh.index = meshIndex++;
        mesh.offset = meshOffset;
        mesh.resourceId = meshResourceIds.count(meshOffset) ? meshResourceIds[meshOffset] : 0;
        mesh.firstVertex = firstVertex;
        mesh.vertexCount = vertexCount;
        mesh.firstTriangle = firstTriangle;
        mesh.triangleCount = triangleCount;
        mesh.materialCount = uint8_t(materialCount);
        mesh.scaleFactor = scaleFactor;
        parsedMeshes.push_back(mesh);
    }

    std::ostringstream summary;
    summary << "CTW WBL worldblock: size=" << rawBytes.size()
            << " bytes / pages=" << pageCount()
            << " origin=(" << headerPosX << ", " << headerPosY << ", " << headerPosZ << ")"
            << " sectors=" << sectors.size()
            << " instance mesh refs=" << meshOffsets.size();
    parsedLines.push_back(summary.str());

    std::ostringstream meshLine;
    meshLine << "Meshes: " << parsedMeshes.size()
             << " parsed, vertices=" << parsedVertices.size()
             << ", triangles=" << parsedTriangles.size();
    parsedLines.push_back(meshLine.str());

    std::ostringstream sectorLine;
    sectorLine << "Sector instances:";
    for (size_t i = 0; i < sectors.size(); ++i) sectorLine << " " << i << "=" << sectors[i].instances;
    parsedLines.push_back(sectorLine.str());
}

const std::wstring& StorylandWblFile::sourcePath() const { return currentPath; }
const std::vector<uint8_t>& StorylandWblFile::bytes() const { return rawBytes; }
const std::vector<StorylandWblBox>& StorylandWblFile::boxes() const { return parsedBoxes; }
const std::vector<StorylandWblSection>& StorylandWblFile::sections() const { return parsedSections; }
const std::vector<StorylandWblVertex>& StorylandWblFile::vertices() const { return parsedVertices; }
const std::vector<StorylandWblTriangle>& StorylandWblFile::triangles() const { return parsedTriangles; }
const std::vector<StorylandWblMesh>& StorylandWblFile::meshes() const { return parsedMeshes; }
const std::vector<std::string>& StorylandWblFile::lines() const { return parsedLines; }
uint64_t StorylandWblFile::fileSize() const { return uint64_t(rawBytes.size()); }
uint32_t StorylandWblFile::pageCount() const { return uint32_t((rawBytes.size() + 0xFFFu) / 0x1000u); }
int32_t StorylandWblFile::originXRaw() const { return headerOriginXRaw; }
int32_t StorylandWblFile::originYRaw() const { return headerOriginYRaw; }
float StorylandWblFile::originX() const { return float(headerOriginXRaw) / 4096.0f; }
float StorylandWblFile::originY() const { return float(headerOriginYRaw) / 4096.0f; }
uint16_t StorylandWblFile::primaryCount() const { return headerPrimaryCount; }
uint16_t StorylandWblFile::secondaryCount() const { return headerSecondaryCount; }
uint16_t StorylandWblFile::tertiaryCount() const { return headerTertiaryCount; }
uint16_t StorylandWblFile::tableA() const { return headerTableA; }
uint16_t StorylandWblFile::tableB() const { return headerTableB; }
uint16_t StorylandWblFile::tableC() const { return headerTableC; }
uint16_t StorylandWblFile::tableD() const { return headerTableD; }
uint16_t StorylandWblFile::tableE() const { return headerTableE; }

bool StorylandWblFile::hasBounds(float& minX, float& minY, float& minZ, float& maxX, float& maxY, float& maxZ) const {
    bool have = false;
    for (const StorylandWblVertex& v : parsedVertices) {
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) continue;
        if (!have) {
            minX = maxX = v.x;
            minY = maxY = v.y;
            minZ = maxZ = v.z;
            have = true;
        } else {
            minX = std::min(minX, v.x); minY = std::min(minY, v.y); minZ = std::min(minZ, v.z);
            maxX = std::max(maxX, v.x); maxY = std::max(maxY, v.y); maxZ = std::max(maxZ, v.z);
        }
    }
    return have;
}
