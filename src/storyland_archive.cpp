#include "storyland_archive.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <zlib.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace {

static constexpr uint32_t TEX_IDENT  = 0x00746578; // "tex\0"
static constexpr uint32_t MDL_IDENT  = 0x006D646C; // "ldm\0" little endian bytes
static constexpr uint32_t WRLD_IDENT = 0x57524C44; // "DLRW"
static constexpr uint32_t AREA_IDENT = 0x41455241; // "AREA"
static constexpr uint32_t AERA_IDENT = 0x41524541; // "AERA" fallback for reversed/debug dumps
static constexpr uint32_t GTAG_IDENT = 0x47544147;

static std::string narrowForMessage(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        if (ch >= 32 && ch < 127) out.push_back(char(ch));
        else out.push_back('?');
    }
    return out;
}

#ifdef _WIN32
static std::string windowsErrorMessage(DWORD errorCode) {
    wchar_t* buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD length = FormatMessageW(
        flags,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );

    std::wstring wide;
    if (length != 0 && buffer != nullptr) {
        wide.assign(buffer, buffer + length);
        while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L' ' || wide.back() == L'\t')) {
            wide.pop_back();
        }
    }

    if (buffer != nullptr) {
        LocalFree(buffer);
    }

    if (wide.empty()) {
        return "Windows error " + std::to_string(errorCode);
    }

    return narrowForMessage(wide) + " (GetLastError=" + std::to_string(errorCode) + ")";
}
#endif

static uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0;
    return uint32_t(bytes[offset + 0]) |
           (uint32_t(bytes[offset + 1]) << 8) |
           (uint32_t(bytes[offset + 2]) << 16) |
           (uint32_t(bytes[offset + 3]) << 24);
}

static void writeU32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    if (offset + 4 > bytes.size()) return;
    bytes[offset + 0] = uint8_t(value & 0xFF);
    bytes[offset + 1] = uint8_t((value >> 8) & 0xFF);
    bytes[offset + 2] = uint8_t((value >> 16) & 0xFF);
    bytes[offset + 3] = uint8_t((value >> 24) & 0xFF);
}

static void writeU16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value) {
    if (offset + 2 > bytes.size()) return;
    bytes[offset + 0] = uint8_t(value & 0xFF);
    bytes[offset + 1] = uint8_t((value >> 8) & 0xFF);
}

static std::wstring lowerWide(const std::wstring& text) {
    std::wstring out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
    return out;
}

static std::wstring getFileStemPart(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    size_t start = slash == std::wstring::npos ? 0 : slash + 1;
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || dot < start) dot = path.size();
    return path.substr(start, dot - start);
}

static std::wstring getExtensionLower(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    return lowerWide(path.substr(dot));
}

static bool isAreaIdent(uint32_t ident) {
    return ident == AREA_IDENT || ident == AERA_IDENT;
}

static bool isWorldLikeIdent(uint32_t ident) {
    return ident == WRLD_IDENT || isAreaIdent(ident);
}

static const char* literalForIdent(uint32_t ident) {
    if (ident == WRLD_IDENT) return "DLRW";
    if (ident == AREA_IDENT) return "AREA";
    if (ident == AERA_IDENT) return "AERA";
    if (ident == MDL_IDENT) return "ldm";
    if (ident == TEX_IDENT) return "tex";
    if (ident == GTAG_IDENT) return "GTAG";
    return "????";
}

static std::string extensionForIdent(uint32_t ident) {
    if (ident == MDL_IDENT) return ".mdl";
    if (ident == TEX_IDENT) return ".xtx";
    if (ident == WRLD_IDENT) return ".wrld";
    if (isAreaIdent(ident)) return ".area";
    if (ident == GTAG_IDENT) return ".dtz";
    return ".bin";
}

static const char* labelForIdent(uint32_t ident) {
    if (ident == MDL_IDENT) return "mdl";
    if (ident == TEX_IDENT) return "texture";
    if (ident == WRLD_IDENT) return "world";
    if (isAreaIdent(ident)) return "area";
    if (ident == GTAG_IDENT) return "gtag";
    return "chunk";
}

static bool knownChunkIdent(uint32_t ident) {
    return ident == MDL_IDENT || ident == TEX_IDENT || ident == WRLD_IDENT || isAreaIdent(ident) || ident == GTAG_IDENT;
}

static uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 2 > bytes.size()) return 0;
    return uint16_t(bytes[offset + 0]) | (uint16_t(bytes[offset + 1]) << 8);
}

static float readF32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0.0f;
    float value = 0.0f;
    std::memcpy(&value, bytes.data() + offset, sizeof(float));
    return value;
}

static float halfToFloat(uint16_t value) {
    uint32_t sign = uint32_t(value & 0x8000) << 16;
    uint32_t exponent = (value >> 10) & 0x1F;
    uint32_t mantissa = value & 0x03FF;
    uint32_t out = 0;

    if (exponent == 0) {
        if (mantissa == 0) {
            out = sign;
        } else {
            exponent = 1;
            while ((mantissa & 0x0400) == 0) {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x03FF;
            uint32_t fexp = exponent + (127 - 15);
            out = sign | (fexp << 23) | (mantissa << 13);
        }
    } else if (exponent == 0x1F) {
        out = sign | 0x7F800000 | (mantissa << 13);
    } else {
        uint32_t fexp = exponent + (127 - 15);
        out = sign | (fexp << 23) | (mantissa << 13);
    }

    float result = 0.0f;
    std::memcpy(&result, &out, sizeof(float));
    return result;
}

static bool finiteReasonable(float value, float limit) {
    return std::isfinite(value) && std::fabs(value) <= limit;
}

static bool looksLikeImgInstanceRow(const std::vector<uint8_t>& img, size_t offset, uint32_t maxResourceId) {
    if (offset + 0x50 > img.size()) return false;

    uint16_t resId = readU16(img, offset + 0x02);
    if (resId == 0 || resId == 0xFFFF) return false;
    if (maxResourceId > 0 && resId >= maxResourceId) return false;

    for (int i = 0; i < 4; ++i) {
        float bound = halfToFloat(readU16(img, offset + 0x04 + size_t(i) * 2u));
        if (!finiteReasonable(bound, 100000.0f)) return false;
    }

    for (int i = 0; i < 16; ++i) {
        float value = readF32(img, offset + 0x10 + size_t(i) * 4u);
        if (!finiteReasonable(value, 100000.0f)) return false;
    }

    float rightX = readF32(img, offset + 0x10 + 0x00);
    float rightY = readF32(img, offset + 0x10 + 0x04);
    float rightZ = readF32(img, offset + 0x10 + 0x08);
    float upX = readF32(img, offset + 0x10 + 0x10);
    float upY = readF32(img, offset + 0x10 + 0x14);
    float upZ = readF32(img, offset + 0x10 + 0x18);
    float atX = readF32(img, offset + 0x10 + 0x20);
    float atY = readF32(img, offset + 0x10 + 0x24);
    float atZ = readF32(img, offset + 0x10 + 0x28);

    float scale0 = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
    float scale1 = std::sqrt(upX * upX + upY * upY + upZ * upZ);
    float scale2 = std::sqrt(atX * atX + atY * atY + atZ * atZ);
    if (!finiteReasonable(scale0, 10000.0f) || !finiteReasonable(scale1, 10000.0f) || !finiteReasonable(scale2, 10000.0f)) return false;
    if (scale0 < 0.00001f || scale1 < 0.00001f || scale2 < 0.00001f) return false;

    return true;
}

static int detectWorldGameRowCount(size_t rowCount) {
    if (rowCount == 47) return 47;
    if (rowCount == 37) return 37;
    return rowCount >= 42 ? 47 : 37;
}

static void sectorOriginForXY(int rowCount, uint32_t sectorX, uint32_t sectorY, float& outX, float& outY, float& outZ) {
    bool lcs = rowCount == 47;
    float xinc = lcs ? 100.0f : 125.0f;
    float yinc = lcs ? 86.6f : 108.25f;
    float xstart = lcs ? -2000.0f : -2400.0f;
    float ystart = -2000.0f;

    outX = xstart + (xinc * 0.5f) + (xinc * float(sectorX)) - ((sectorY & 1u) ? xinc * 0.5f : 0.0f);
    outY = ystart + (yinc * 0.5f) + (yinc * float(sectorY));
    outZ = 0.0f;
}

static const char* passNameForIndex(int rowCount, size_t passIndex) {
    static const char* vcsNames[] = {
        "SUPERLOD", "UNDERWATER", "LOD", "ROADS", "NORMAL", "NOZWRITE", "LIGHTS", "TRANSPARENT"
    };
    static const char* lcsNames[] = {
        "SUPERLOD", "LOD", "ROADS", "NORMAL", "NOZWRITE", "LIGHTS", "TRANSPARENT"
    };
    if (rowCount == 47) {
        return passIndex < (sizeof(lcsNames) / sizeof(lcsNames[0])) ? lcsNames[passIndex] : "PASS";
    }
    return passIndex < (sizeof(vcsNames) / sizeof(vcsNames[0])) ? vcsNames[passIndex] : "PASS";
}

static bool passIsLod(const char* name) {
    return std::strcmp(name, "SUPERLOD") == 0 || std::strcmp(name, "LOD") == 0;
}

static bool resolveWorldRowHeaderAddress(const std::vector<uint8_t>& lvz, uint32_t masterBase, uint32_t rawAddress, uint32_t& resolvedAddress) {
    if (rawAddress == 0 || rawAddress == 0xFFFFFFFFu) return false;

    uint32_t candidates[2] = {
        rawAddress,
        rawAddress <= 0x7FFFFFFFu && masterBase <= 0x7FFFFFFFu ? masterBase + rawAddress : rawAddress
    };

    for (uint32_t candidate : candidates) {
        if ((candidate & 3u) != 0) continue;
        if (uint64_t(candidate) + 0x20ull > lvz.size()) continue;

        uint32_t ident = readU32(lvz, candidate);
        if (!isWorldLikeIdent(ident)) continue;

        uint32_t fileSize = readU32(lvz, candidate + 0x08);
        if (fileSize < 0x20 || fileSize > 0x08000000u) continue;

        resolvedAddress = candidate;
        return true;
    }

    return false;
}

static bool tryReadSectorRowsFromWorldHeaderAt(const std::vector<uint8_t>& lvz, uint32_t masterBase, std::vector<std::pair<uint32_t, uint32_t>>& rows) {
    rows.clear();

    if (uint64_t(masterBase) + 0x24ull > lvz.size()) return false;
    uint32_t masterIdent = readU32(lvz, masterBase);
    if (!isWorldLikeIdent(masterIdent)) return false;

    size_t cursor = size_t(masterBase) + 0x24u;
    while (cursor + 8 <= lvz.size()) {
        uint32_t rawHeaderAddress = readU32(lvz, cursor + 0);
        uint32_t startOff = readU32(lvz, cursor + 4);

        uint32_t resolvedHeaderAddress = 0;
        if (!resolveWorldRowHeaderAddress(lvz, masterBase, rawHeaderAddress, resolvedHeaderAddress)) break;

        // VCS AREA may sit in the same header stream as WRLD.  Keep it in the
        // row list so sector traversal does not silently stop when the AREA
        // linked chunk is encountered before/around regular WRLD rows.
        rows.push_back({resolvedHeaderAddress, startOff});

        cursor += 8;
        if (rows.size() > 96) break;
    }

    return rows.size() >= 2;
}

static bool readSectorRowsFromLvz(const std::vector<uint8_t>& lvz, std::vector<std::pair<uint32_t, uint32_t>>& rows) {
    rows.clear();
    if (lvz.size() < 0x24) return false;

    std::vector<uint32_t> candidates;

    uint32_t topIdent = readU32(lvz, 0);
    if (isWorldLikeIdent(topIdent)) candidates.push_back(0);

    // VCS can wrap/precede normal WRLD data with AREA.  Do not assume the master
    // WRLD row table begins at offset zero; scan early LVZ memory for a valid
    // master-like WRLD/AREA header and validate by following its row directory.
    size_t scanLimit = std::min<size_t>(lvz.size(), 2u * 1024u * 1024u);
    for (size_t offset = 0; offset + 0x24 <= scanLimit; offset += 4) {
        uint32_t ident = readU32(lvz, offset);
        if (!isWorldLikeIdent(ident)) continue;

        uint32_t typeOrFlags = readU32(lvz, offset + 0x04);
        if (ident == WRLD_IDENT && typeOrFlags != 1u && offset != 0) {
            // Slave/triggered WRLD headers also appear everywhere.  Prioritize
            // master headers, but still allow offset 0 because old LCS/VCS LVZ
            // files start with the master WRLD there.
            continue;
        }

        candidates.push_back(uint32_t(offset));
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    for (uint32_t base : candidates) {
        if (tryReadSectorRowsFromWorldHeaderAt(lvz, base, rows)) return true;
    }

    return false;
}

static int16_t readI16(const std::vector<uint8_t>& bytes, size_t offset) {
    return int16_t(readU16(bytes, offset));
}

static int32_t readI32(const std::vector<uint8_t>& bytes, size_t offset) {
    return int32_t(readU32(bytes, offset));
}

static size_t alignUp4Size(size_t value) {
    return (value + 3u) & ~size_t(3u);
}

static size_t alignDown4Size(size_t value) {
    return value & ~size_t(3u);
}

static size_t findUnpackNear(const std::vector<uint8_t>& bytes, size_t offset, size_t maxEnd, size_t window = 8) {
    size_t start = offset > window ? offset - window : 0;
    size_t stop = std::min(maxEnd, offset + window + 4);
    for (size_t cursor = start; cursor + 4 <= stop; ++cursor) {
        if ((cursor & 3u) == 0 && readU32(bytes, cursor) == 0x6C018000u) return cursor;
    }
    return SIZE_MAX;
}

struct ParsedWorldMaterial {
    uint32_t textureId = 0xFFFFFFFFu;
    uint32_t stripByteBudget = 0;
    float uScale = 1.0f;
    float vScale = 1.0f;
};

struct ParsedWorldMaterialList {
    std::vector<ParsedWorldMaterial> materials;
    size_t streamStart = 0;
    size_t listEnd = 0;
    uint32_t rowLength = 0;
};

struct ParsedWorldStrip {
    std::vector<StorylandWorldMeshVertex> vertices;
    uint32_t streamBytes = 0;
    uint32_t textureId = 0xFFFFFFFFu;
};

static bool parseWorldMaterialList(
    const std::vector<uint8_t>& bytes,
    size_t base,
    size_t maxEnd,
    ParsedWorldMaterialList& outList
) {
    outList = {};
    if (base + 4 > maxEnd || maxEnd > bytes.size()) return false;

    uint32_t count = readU16(bytes, base + 0);
    uint32_t sizeBytes = readU16(bytes, base + 2);
    if (count == 0 || count > 256) return false;
    if (sizeBytes < count * 22u || sizeBytes > 0x4000u) return false;
    if (base + 4u + sizeBytes > maxEnd) return false;

    uint32_t rowLength = 0;
    if (sizeBytes >= count * 24u) rowLength = 24;
    else if (sizeBytes >= count * 22u) rowLength = 22;
    else return false;

    size_t rowsBase = base + 4u;
    for (uint32_t index = 0; index < count; ++index) {
        size_t row = rowsBase + size_t(index) * rowLength;
        if (row + rowLength > maxEnd) return false;

        ParsedWorldMaterial material;
        if (rowLength == 24) {
            uint32_t packetRaw = readU32(bytes, row + 0);
            material.stripByteBudget = packetRaw >> 1;
            material.textureId = readU16(bytes, row + 4);
            material.uScale = halfToFloat(readU16(bytes, row + 6));
            material.vScale = halfToFloat(readU16(bytes, row + 8));
        } else {
            material.textureId = readU16(bytes, row + 0);
            uint32_t packetRaw = readU16(bytes, row + 2);
            material.stripByteBudget = packetRaw & 0x7FFFu;
            material.uScale = halfToFloat(readU16(bytes, row + 4));
            material.vScale = halfToFloat(readU16(bytes, row + 6));
        }
        if (!std::isfinite(material.uScale) || std::fabs(material.uScale) > 4096.0f || material.uScale == 0.0f) material.uScale = 1.0f;
        if (!std::isfinite(material.vScale) || std::fabs(material.vScale) > 4096.0f || material.vScale == 0.0f) material.vScale = 1.0f;
        outList.materials.push_back(material);
    }

    outList.rowLength = rowLength;
    outList.listEnd = base + 4u + sizeBytes;
    size_t cursor = outList.listEnd;
    while (cursor < maxEnd && bytes[cursor] == 0xAA && cursor - outList.listEnd < 0x1000u) cursor++;
    outList.streamStart = alignUp4Size(cursor);
    return !outList.materials.empty();
}

static bool parseOneWorldVifStrip(
    const std::vector<uint8_t>& bytes,
    size_t requestedOffset,
    size_t maxEnd,
    ParsedWorldStrip& outStrip
) {
    outStrip = {};
    size_t pos = findUnpackNear(bytes, alignDown4Size(requestedOffset), maxEnd);
    if (pos == SIZE_MAX || pos + 20 > maxEnd) return false;

    uint32_t vertexCount = readU32(bytes, pos + 16) & 0x7FFFu;
    if (vertexCount < 3 || vertexCount > 8192) return false;

    size_t cursor = pos + 20;
    if (cursor + 8 > maxEnd || readU32(bytes, cursor) != 0x20000000u) return false;
    cursor += 8;
    if (cursor + 20 > maxEnd || readU32(bytes, cursor) != 0x30000000u) return false;
    cursor += 20;
    uint32_t positionHeader = readU32(bytes, cursor);
    if (((positionHeader >> 24) & 0x7Fu) != 0x79u) return false;
    cursor += 4;

    size_t positionBytes = size_t(vertexCount) * 6u;
    if (cursor + positionBytes > maxEnd) return false;

    outStrip.vertices.resize(vertexCount);
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        size_t vertexOffset = cursor + size_t(vertexIndex) * 6u;
        outStrip.vertices[vertexIndex].x = float(readI16(bytes, vertexOffset + 0)) / 32767.5f;
        outStrip.vertices[vertexIndex].y = float(readI16(bytes, vertexOffset + 2)) / 32767.5f;
        outStrip.vertices[vertexIndex].z = float(readI16(bytes, vertexOffset + 4)) / 32767.5f;
    }
    cursor = alignUp4Size(cursor + positionBytes);

    if (cursor + 8 > maxEnd || readU32(bytes, cursor) != 0x20000000u) return false;
    cursor += 8;
    if (cursor + 20 > maxEnd || readU32(bytes, cursor) != 0x30000000u) return false;
    cursor += 20;
    uint32_t uvHeader = readU32(bytes, cursor);
    if (((uvHeader >> 24) & 0x7Fu) != 0x76u) return false;
    cursor += 4;

    size_t uvBytes = size_t(vertexCount) * 2u;
    if (cursor + uvBytes > maxEnd) return false;
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        size_t uvOffset = cursor + size_t(vertexIndex) * 2u;
        outStrip.vertices[vertexIndex].u = float(bytes[uvOffset + 0]) / 255.0f;
        outStrip.vertices[vertexIndex].v = float(bytes[uvOffset + 1]) / 255.0f;
    }
    cursor = alignUp4Size(cursor + uvBytes);

    if (cursor + 4 > maxEnd) return false;
    uint32_t colorHeader = readU32(bytes, cursor);
    if (((colorHeader >> 24) & 0x7Fu) != 0x6Fu) return false;
    cursor += 4;
    size_t colorBytes = size_t(vertexCount) * 2u;
    if (cursor + colorBytes > maxEnd) return false;
    cursor = alignUp4Size(cursor + colorBytes);

    if (cursor + 4 <= maxEnd && readU32(bytes, cursor) == 0x14000006u) {
        cursor += 4;
    } else if (cursor + 8 <= maxEnd && readU32(bytes, cursor + 4) == 0x14000006u) {
        cursor += 8;
    }
    while (cursor + 4 <= maxEnd && readU32(bytes, cursor) == 0) cursor += 4;

    outStrip.streamBytes = uint32_t(cursor - pos);
    return true;
}

static bool parseWorldOverlayMesh(
    const std::vector<uint8_t>& bytes,
    size_t rawOffset,
    size_t maxEnd,
    uint32_t sectorIndex,
    uint32_t resourceIndex,
    StorylandWorldMesh& outMesh
) {
    outMesh = {};
    ParsedWorldMaterialList materialList;
    if (!parseWorldMaterialList(bytes, rawOffset, maxEnd, materialList)) return false;

    std::vector<ParsedWorldStrip> strips;
    size_t cursor = materialList.streamStart;
    size_t stripSearchLimit = std::min(maxEnd, rawOffset + 0x200000u);
    uint32_t totalVertices = 0;

    for (uint32_t groupIndex = 0; groupIndex < 512 && cursor < stripSearchLimit; ++groupIndex) {
        size_t unpack = findUnpackNear(bytes, cursor, stripSearchLimit, 32);
        if (unpack == SIZE_MAX) break;
        ParsedWorldStrip strip;
        if (!parseOneWorldVifStrip(bytes, unpack, stripSearchLimit, strip)) break;
        if (strip.vertices.empty()) break;
        totalVertices += uint32_t(strip.vertices.size());
        if (totalVertices > 250000u) break;
        strips.push_back(std::move(strip));
        cursor = unpack + strips.back().streamBytes;
    }
    if (strips.empty()) return false;

    uint32_t materialIndex = 0;
    uint32_t materialBudget = materialList.materials.empty() ? 0 : materialList.materials[0].stripByteBudget;
    uint32_t accumulatedBytes = 0;

    outMesh.sectorIndex = sectorIndex;
    outMesh.resourceIndex = resourceIndex;
    outMesh.rawOffset = rawOffset;
    outMesh.materialCount = uint32_t(materialList.materials.size());

    for (const ParsedWorldStrip& strip : strips) {
        while (materialIndex + 1 < materialList.materials.size() && materialBudget > 0 && accumulatedBytes >= materialBudget) {
            materialIndex++;
            materialBudget = materialList.materials[materialIndex].stripByteBudget;
            accumulatedBytes = 0;
        }

        uint32_t textureId = 0xFFFFFFFFu;
        float uScale = 1.0f;
        float vScale = 1.0f;
        if (materialIndex < materialList.materials.size()) {
            textureId = materialList.materials[materialIndex].textureId;
            uScale = materialList.materials[materialIndex].uScale;
            vScale = materialList.materials[materialIndex].vScale;
        }

        uint32_t baseVertex = uint32_t(outMesh.vertices.size());
        for (StorylandWorldMeshVertex vertex : strip.vertices) {
            vertex.u *= uScale;
            vertex.v *= vScale;
            outMesh.vertices.push_back(vertex);
        }

        if (strip.vertices.size() >= 3) {
            for (uint32_t index = 0; index + 2 < strip.vertices.size(); ++index) {
                StorylandWorldMeshTriangle tri;
                if (index & 1u) {
                    tri.a = baseVertex + index + 1;
                    tri.b = baseVertex + index + 0;
                    tri.c = baseVertex + index + 2;
                } else {
                    tri.a = baseVertex + index + 0;
                    tri.b = baseVertex + index + 1;
                    tri.c = baseVertex + index + 2;
                }
                tri.textureId = textureId;
                outMesh.triangles.push_back(tri);
            }
        }

        accumulatedBytes += strip.streamBytes;
    }

    return !outMesh.vertices.empty() && !outMesh.triangles.empty();
}


static void appendWorldPayloadU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(uint8_t(value & 0xFF));
    out.push_back(uint8_t((value >> 8) & 0xFF));
}

static void appendWorldPayloadI16(std::vector<uint8_t>& out, int16_t value) {
    appendWorldPayloadU16(out, uint16_t(value));
}

static void appendWorldPayloadU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(uint8_t(value & 0xFF));
    out.push_back(uint8_t((value >> 8) & 0xFF));
    out.push_back(uint8_t((value >> 16) & 0xFF));
    out.push_back(uint8_t((value >> 24) & 0xFF));
}

static void alignWorldPayload4(std::vector<uint8_t>& out) {
    while (out.size() & 3u) out.push_back(0);
}

static uint8_t clampByteFromUnit(float value) {
    if (!std::isfinite(value)) return 0;
    value = std::max(0.0f, std::min(1.0f, value));
    return uint8_t(std::round(value * 255.0f));
}

static int16_t clampI16FromUnitSigned(float value) {
    if (!std::isfinite(value)) return 0;
    value = std::max(-1.0f, std::min(1.0f, value));
    return int16_t(std::round(value * 32767.0f));
}

static void appendGeneratedWorldVifStrip(std::vector<uint8_t>& out, const ParsedWorldStrip& strip) {
    uint32_t vertexCount = uint32_t(std::min<size_t>(strip.vertices.size(), 255));
    if (vertexCount < 3) return;

    appendWorldPayloadU32(out, 0x6C018000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, vertexCount);

    appendWorldPayloadU32(out, 0x20000000u);
    appendWorldPayloadU32(out, 0x00000000u);

    appendWorldPayloadU32(out, 0x30000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);

    appendWorldPayloadU32(out, (0x79u << 24) | ((vertexCount & 0xFFu) << 16) | 0x4000u);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        appendWorldPayloadI16(out, clampI16FromUnitSigned(strip.vertices[i].x));
        appendWorldPayloadI16(out, clampI16FromUnitSigned(strip.vertices[i].y));
        appendWorldPayloadI16(out, clampI16FromUnitSigned(strip.vertices[i].z));
    }
    alignWorldPayload4(out);

    appendWorldPayloadU32(out, 0x20000000u);
    appendWorldPayloadU32(out, 0x00000000u);

    appendWorldPayloadU32(out, 0x30000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);
    appendWorldPayloadU32(out, 0x00000000u);

    appendWorldPayloadU32(out, (0x76u << 24) | ((vertexCount & 0xFFu) << 16) | 0x4000u);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        appendWorldPayloadU16(out, uint16_t(clampByteFromUnit(strip.vertices[i].u)) | (uint16_t(clampByteFromUnit(strip.vertices[i].v)) << 8));
    }
    alignWorldPayload4(out);

    appendWorldPayloadU32(out, (0x6Fu << 24) | ((vertexCount & 0xFFu) << 16) | 0x4000u);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        appendWorldPayloadU16(out, 0x7FFFu);
    }
    alignWorldPayload4(out);

    appendWorldPayloadU32(out, 0x14000006u);
    alignWorldPayload4(out);
}

static bool stripFallbackHasShape(const ParsedWorldStrip& strip) {
    if (strip.vertices.size() < 3) return false;

    float minX = strip.vertices[0].x;
    float minY = strip.vertices[0].y;
    float minZ = strip.vertices[0].z;
    float maxX = minX;
    float maxY = minY;
    float maxZ = minZ;

    for (const StorylandWorldMeshVertex& vertex : strip.vertices) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) return false;
        minX = std::min(minX, vertex.x); maxX = std::max(maxX, vertex.x);
        minY = std::min(minY, vertex.y); maxY = std::max(maxY, vertex.y);
        minZ = std::min(minZ, vertex.z); maxZ = std::max(maxZ, vertex.z);
    }

    float span = std::fabs(maxX - minX) + std::fabs(maxY - minY) + std::fabs(maxZ - minZ);
    return std::isfinite(span) && span > 0.0001f;
}

static bool tryReadLeedsMarkerFallbackStrip(
    const std::vector<uint8_t>& bytes,
    size_t markerOffset,
    ParsedWorldStrip& outStrip
) {
    outStrip = {};
    if (markerOffset + 0x34 > bytes.size()) return false;
    if (readU32(bytes, markerOffset) != 0x6C018000u) return false;

    uint32_t vertexCount = bytes[markerOffset + 0x32];
    if (vertexCount < 3 || vertexCount > 128) return false;

    size_t vertexDataOffset = markerOffset + 0x34;
    size_t vertexDataBytes = size_t(vertexCount) * 6u;
    if (vertexDataOffset + vertexDataBytes > bytes.size()) return false;

    outStrip.vertices.reserve(vertexCount);
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        size_t vertexOffset = vertexDataOffset + size_t(vertexIndex) * 6u;

        StorylandWorldMeshVertex vertex;
        vertex.x = float(readI16(bytes, vertexOffset + 0));
        vertex.y = float(readI16(bytes, vertexOffset + 2));
        vertex.z = float(readI16(bytes, vertexOffset + 4));

        // Best-effort UV fallback.  Some MDL strip blocks put UV after a small
        // VIF row/column prelude; this generic path does not rely on that.
        // It is better to accept the MDL and preserve/replace the resource than
        // refuse the file or make the WRLD disappear.
        vertex.u = (vertexIndex & 1u) ? 1.0f : 0.0f;
        vertex.v = (vertexIndex & 2u) ? 1.0f : 0.0f;

        outStrip.vertices.push_back(vertex);
    }

    if (!stripFallbackHasShape(outStrip)) return false;

    outStrip.streamBytes = uint32_t(0x34u + vertexDataBytes);
    return true;
}

static void normalizeFallbackStripsToWorldUnit(std::vector<ParsedWorldStrip>& strips) {
    if (strips.empty()) return;

    bool haveAny = false;
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;

    for (const ParsedWorldStrip& strip : strips) {
        for (const StorylandWorldMeshVertex& vertex : strip.vertices) {
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) continue;

            if (!haveAny) {
                minX = maxX = vertex.x;
                minY = maxY = vertex.y;
                minZ = maxZ = vertex.z;
                haveAny = true;
            } else {
                minX = std::min(minX, vertex.x); maxX = std::max(maxX, vertex.x);
                minY = std::min(minY, vertex.y); maxY = std::max(maxY, vertex.y);
                minZ = std::min(minZ, vertex.z); maxZ = std::max(maxZ, vertex.z);
            }
        }
    }

    if (!haveAny) return;

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;

    float spanX = std::fabs(maxX - minX);
    float spanY = std::fabs(maxY - minY);
    float spanZ = std::fabs(maxZ - minZ);
    float span = std::max(spanX, std::max(spanY, spanZ));
    float scale = span > 0.0001f ? (2.0f / span) : (1.0f / 32767.0f);

    for (ParsedWorldStrip& strip : strips) {
        for (StorylandWorldMeshVertex& vertex : strip.vertices) {
            vertex.x = (vertex.x - centerX) * scale;
            vertex.y = (vertex.y - centerY) * scale;
            vertex.z = (vertex.z - centerZ) * scale;

            vertex.x = std::max(-1.0f, std::min(1.0f, vertex.x));
            vertex.y = std::max(-1.0f, std::min(1.0f, vertex.y));
            vertex.z = std::max(-1.0f, std::min(1.0f, vertex.z));
        }
    }
}

static bool collectFallbackLeedsMarkerStrips(const std::vector<uint8_t>& bytes, std::vector<ParsedWorldStrip>& strips) {
    strips.clear();
    if (bytes.size() < 0x40) return false;

    size_t cursor = 0;
    if (bytes.size() >= 0x20 && knownChunkIdent(readU32(bytes, 0x00))) cursor = 0x20;

    uint32_t totalVertices = 0;

    while (cursor + 0x34 <= bytes.size() && strips.size() < 4096) {
        if ((cursor & 3u) != 0 || readU32(bytes, cursor) != 0x6C018000u) {
            cursor += 4;
            continue;
        }

        ParsedWorldStrip strip;
        if (tryReadLeedsMarkerFallbackStrip(bytes, cursor, strip)) {
            totalVertices += uint32_t(strip.vertices.size());
            if (totalVertices > 250000u) break;

            size_t nextCursor = cursor + strip.streamBytes;
            strips.push_back(std::move(strip));
            cursor = std::max(cursor + 4, alignUp4Size(nextCursor));
            continue;
        }

        cursor += 4;
    }

    if (strips.empty()) return false;
    normalizeFallbackStripsToWorldUnit(strips);
    return true;
}

static bool collectLeedsStripsFromAnyChunk(const std::vector<uint8_t>& bytes, std::vector<ParsedWorldStrip>& strips) {
    strips.clear();
    if (bytes.size() < 0x40) return false;

    size_t start = 0;
    if (bytes.size() >= 0x20 && knownChunkIdent(readU32(bytes, 0x00))) start = 0x20;

    size_t maxEnd = bytes.size();
    size_t cursor = start;
    uint32_t totalVertices = 0;

    while (cursor + 0x80 < maxEnd && strips.size() < 4096) {
        size_t unpack = findUnpackNear(bytes, cursor, maxEnd, 16);
        if (unpack == SIZE_MAX) break;

        ParsedWorldStrip strip;
        if (parseOneWorldVifStrip(bytes, unpack, maxEnd, strip) && strip.vertices.size() >= 3) {
            totalVertices += uint32_t(strip.vertices.size());
            if (totalVertices > 250000u) break;
            strips.push_back(std::move(strip));
            cursor = unpack + strips.back().streamBytes;
        } else {
            cursor = unpack + 4;
        }
    }

    if (!strips.empty()) return true;

    // Normal MDLs are not sector payloads.  They often still contain the Leeds
    // split marker layout used by the MDL viewer: 0x6C018000, vertex count at
    // +0x32, and packed xyz at +0x34.  Accept that too and convert it into the
    // sector payload Storyland needs instead of refusing the .mdl.
    return collectFallbackLeedsMarkerStrips(bytes, strips);
}

struct LeedsExactStripSpan {
    size_t offset = 0;
    size_t size = 0;
    uint32_t vertexCount = 0;
};

static bool collectExactLeedsStripSpansFromAnyChunk(const std::vector<uint8_t>& bytes, std::vector<LeedsExactStripSpan>& spans) {
    spans.clear();
    if (bytes.size() < 0x40) return false;

    size_t start = 0;
    if (bytes.size() >= 0x20 && knownChunkIdent(readU32(bytes, 0x00))) start = 0x20;

    size_t maxEnd = bytes.size();
    size_t cursor = start;
    uint32_t totalVertices = 0;

    while (cursor + 0x80 < maxEnd && spans.size() < 4096) {
        size_t unpack = findUnpackNear(bytes, cursor, maxEnd, 16);
        if (unpack == SIZE_MAX) break;

        ParsedWorldStrip strip;
        if (parseOneWorldVifStrip(bytes, unpack, maxEnd, strip) && strip.vertices.size() >= 3 && strip.streamBytes >= 0x40) {
            if (unpack + strip.streamBytes > maxEnd) break;

            totalVertices += uint32_t(strip.vertices.size());
            if (totalVertices > 250000u) break;

            LeedsExactStripSpan span;
            span.offset = unpack;
            span.size = strip.streamBytes;
            span.vertexCount = uint32_t(strip.vertices.size());
            spans.push_back(span);

            cursor = unpack + strip.streamBytes;
        } else {
            cursor = unpack + 4;
        }
    }

    return !spans.empty();
}

static bool buildWorldSectorMeshPayloadFromExactLeedsStripBytes(
    const std::vector<uint8_t>& sourceBytes,
    uint32_t fallbackTextureId,
    std::vector<uint8_t>& outPayload
) {
    outPayload.clear();

    std::vector<LeedsExactStripSpan> spans;
    if (!collectExactLeedsStripSpansFromAnyChunk(sourceBytes, spans)) return false;

    size_t totalStreamBytes = 0;
    for (const LeedsExactStripSpan& span : spans) {
        if (span.offset + span.size > sourceBytes.size()) return false;
        if (span.size > 0x7FFFu) return false;
        totalStreamBytes += alignUp4Size(span.size);
        if (totalStreamBytes > 0x7FFFFFFFu) return false;
    }

    uint16_t materialCount = uint16_t(std::min<size_t>(spans.size(), 0xFFFFu));
    uint16_t materialRowBytes = 24;
    uint32_t materialSizeBytes = uint32_t(materialCount) * uint32_t(materialRowBytes);
    if (materialSizeBytes > 0xFFFFu) return false;

    appendWorldPayloadU16(outPayload, materialCount);
    appendWorldPayloadU16(outPayload, uint16_t(materialSizeBytes));

    for (const LeedsExactStripSpan& span : spans) {
        uint32_t packetRaw = uint32_t(span.size) << 1;
        appendWorldPayloadU32(outPayload, packetRaw);
        appendWorldPayloadU16(outPayload, uint16_t(fallbackTextureId == 0xFFFFFFFFu ? 0 : fallbackTextureId));
        appendWorldPayloadU16(outPayload, 0x3C00u);
        appendWorldPayloadU16(outPayload, 0x3C00u);
        for (int i = 0; i < int(materialRowBytes) - 10; ++i) {
            outPayload.push_back(0);
        }
    }

    alignWorldPayload4(outPayload);

    for (const LeedsExactStripSpan& span : spans) {
        outPayload.insert(outPayload.end(), sourceBytes.begin() + span.offset, sourceBytes.begin() + span.offset + span.size);
        alignWorldPayload4(outPayload);
    }

    StorylandWorldMesh testMesh;
    return parseWorldOverlayMesh(outPayload, 0, outPayload.size(), 0, 0, testMesh) &&
           !testMesh.vertices.empty() &&
           !testMesh.triangles.empty();
}

static bool buildWorldSectorMeshPayloadFromLeedsChunk(
    const std::vector<uint8_t>& sourceBytes,
    uint32_t fallbackTextureId,
    std::vector<uint8_t>& outPayload
) {
    outPayload.clear();

    // First choice: do not decode/re-encode the MDL VIF. Copy the exact Leeds
    // strip packets from the MDL into a sector-resource sBuildingGeometry-style
    // wrapper. Re-encoding was the TLB/VIF risk path.
    if (buildWorldSectorMeshPayloadFromExactLeedsStripBytes(sourceBytes, fallbackTextureId, outPayload)) {
        return true;
    }

    // Fallback for odd files where the viewer can recover vertices but exact VIF
    // packet spans are not laid out cleanly. This is kept as a last resort only.
    std::vector<ParsedWorldStrip> strips;
    if (!collectLeedsStripsFromAnyChunk(sourceBytes, strips)) return false;

    std::vector<uint8_t> stream;
    for (const ParsedWorldStrip& strip : strips) {
        appendGeneratedWorldVifStrip(stream, strip);
    }
    if (stream.empty()) return false;

    uint16_t materialRowBytes = 24;
    appendWorldPayloadU16(outPayload, 1);
    appendWorldPayloadU16(outPayload, materialRowBytes);

    uint32_t packetRaw = uint32_t(std::min<size_t>(stream.size(), 0x7FFFFFFFu)) << 1;
    appendWorldPayloadU32(outPayload, packetRaw);
    appendWorldPayloadU16(outPayload, uint16_t(fallbackTextureId == 0xFFFFFFFFu ? 0 : fallbackTextureId));
    appendWorldPayloadU16(outPayload, 0x3C00u);
    appendWorldPayloadU16(outPayload, 0x3C00u);
    for (int i = 0; i < int(materialRowBytes) - 10; ++i) outPayload.push_back(0);

    alignWorldPayload4(outPayload);
    outPayload.insert(outPayload.end(), stream.begin(), stream.end());
    alignWorldPayload4(outPayload);

    StorylandWorldMesh testMesh;
    return parseWorldOverlayMesh(outPayload, 0, outPayload.size(), 0, 0, testMesh);
}


static bool sourceLooksLikeLeedsChunk(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 0x20) return false;
    uint32_t ident = readU32(bytes, 0x00);
    uint32_t fileSize = readU32(bytes, 0x08);
    if (!knownChunkIdent(ident) && !isAreaIdent(ident)) return false;
    if (fileSize < 0x20 || fileSize > bytes.size()) return false;
    return true;
}

static uint32_t sourceChunkIdentOrMdl(const std::vector<uint8_t>& bytes) {
    if (sourceLooksLikeLeedsChunk(bytes)) return readU32(bytes, 0x00);
    return MDL_IDENT;
}

static std::vector<uint8_t> sourceChunkPayloadForLvzStyle(const std::vector<uint8_t>& bytes) {
    if (sourceLooksLikeLeedsChunk(bytes)) {
        uint32_t fileSize = readU32(bytes, 0x08);
        return std::vector<uint8_t>(bytes.begin() + 0x20, bytes.begin() + fileSize);
    }
    return bytes;
}

static std::vector<uint8_t> sourceChunkHeaderForLvzStyle(const std::vector<uint8_t>& bytes, uint32_t ident, uint64_t imgPayloadOffset, size_t payloadSize) {
    std::vector<uint8_t> header(0x20, 0);

    if (sourceLooksLikeLeedsChunk(bytes)) {
        std::copy(bytes.begin(), bytes.begin() + 0x20, header.begin());
    }

    uint32_t fileSize = uint32_t(payloadSize + 0x20u);
    writeU32(header, 0x00, ident);
    writeU32(header, 0x08, fileSize);

    uint32_t dataSize = readU32(header, 0x0C);
    if (dataSize == 0 || dataSize > fileSize) {
        writeU32(header, 0x0C, uint32_t(payloadSize));
    }

    // LVZ+IMG chunk headers keep the payload byte offset in IMG here.
    writeU32(header, 0x18, uint32_t(imgPayloadOffset));
    return header;
}

static void appendLvzStyleChunkHeader(std::vector<uint8_t>& lvzBytes, const std::vector<uint8_t>& header) {
    while (lvzBytes.size() & 3u) lvzBytes.push_back(0);
    lvzBytes.insert(lvzBytes.end(), header.begin(), header.end());

    if (lvzBytes.size() >= 0x0C) {
        writeU32(lvzBytes, 0x08, uint32_t(lvzBytes.size()));
    }
}

static bool buildPreviewMeshFromLeedsStrips(
    const std::vector<uint8_t>& bytes,
    size_t rawOffset,
    size_t maxEnd,
    uint32_t sectorIndex,
    uint32_t resourceIndex,
    StorylandWorldMesh& outMesh
) {
    outMesh = {};
    if (rawOffset >= maxEnd || maxEnd > bytes.size()) return false;

    std::vector<uint8_t> local(bytes.begin() + rawOffset, bytes.begin() + maxEnd);
    std::vector<ParsedWorldStrip> strips;
    if (!collectLeedsStripsFromAnyChunk(local, strips)) return false;

    outMesh.sectorIndex = sectorIndex;
    outMesh.resourceIndex = resourceIndex;
    outMesh.rawOffset = rawOffset;
    outMesh.materialCount = 0;

    for (const ParsedWorldStrip& strip : strips) {
        if (strip.vertices.size() < 3) continue;

        uint32_t baseVertex = uint32_t(outMesh.vertices.size());
        for (const StorylandWorldMeshVertex& vertex : strip.vertices) {
            outMesh.vertices.push_back(vertex);
        }

        for (uint32_t index = 0; index + 2 < strip.vertices.size(); ++index) {
            StorylandWorldMeshTriangle tri;
            if (index & 1u) {
                tri.a = baseVertex + index + 1;
                tri.b = baseVertex + index + 0;
                tri.c = baseVertex + index + 2;
            } else {
                tri.a = baseVertex + index + 0;
                tri.b = baseVertex + index + 1;
                tri.c = baseVertex + index + 2;
            }
            tri.textureId = 0xFFFFFFFFu;
            outMesh.triangles.push_back(tri);
        }
    }

    return !outMesh.vertices.empty() && !outMesh.triangles.empty();
}



static uint32_t directTextureSwizzlePs2Index(int x, int y, int logw) {
    uint32_t nx = uint32_t(x & 7) ^ (uint32_t(((y >> 1) ^ (y >> 2)) << 2));
    nx = (nx & 7) | (uint32_t((x >> 1) & ~7));
    uint32_t ny = uint32_t(y & 1) | uint32_t((y >> 1) & ~1);
    uint32_t n = uint32_t((y >> 1) & 1) | (uint32_t((x >> 3) & 1) << 1);
    return n | (nx << 2) | (ny << uint32_t(logw - 1 + 2));
}

static int directTextureLog2Width(int width) {
    int logw = 0;
    int value = 1;
    while (value < width) {
        value <<= 1;
        ++logw;
    }
    return logw;
}

static std::vector<uint8_t> directTextureExpandNibblesLoFirst(const uint8_t* bytes, size_t byteCount, size_t pixelCount) {
    std::vector<uint8_t> indices(pixelCount, 0);
    for (size_t i = 0; i < pixelCount; ++i) {
        if (i / 2 >= byteCount) break;
        uint8_t b = bytes[i / 2];
        indices[i] = (i & 1) ? uint8_t(b >> 4) : uint8_t(b & 0x0F);
    }
    return indices;
}

static std::vector<uint8_t> directTextureUnswizzlePs2Indices(const std::vector<uint8_t>& indices, int width, int height) {
    std::vector<uint8_t> out(size_t(width) * size_t(height), 0);
    if (indices.empty()) return out;
    int logw = directTextureLog2Width(width);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t source = directTextureSwizzlePs2Index(x, y, logw);
            out[size_t(y) * size_t(width) + size_t(x)] = indices[source % indices.size()];
        }
    }
    return out;
}

struct StorylandDirectTextureLayout {
    size_t dataStart = 0;
    size_t rasterBytes = 0;
    size_t paletteBytes = 0;
    int width = 0;
    int height = 0;
    int bpp = 0;
    bool swizzled = false;
};

static bool directPaletteLooksUseful(const std::vector<uint8_t>& bytes, size_t paletteStart, size_t paletteBytes) {
    if (paletteBytes == 0) return true;
    if (paletteStart + paletteBytes > bytes.size()) return false;

    uint32_t first = readU32(bytes, paletteStart);
    int meaningful = 0;
    int varied = 0;

    for (size_t offset = 0; offset + 4 <= paletteBytes; offset += 4) {
        uint32_t value = readU32(bytes, paletteStart + offset);
        if (value != 0x00000000u && value != 0xAAAAAAAAu && value != 0xCCCCCCCCu && value != 0xFFFFFFFFu) {
            meaningful++;
        }
        if (value != first) {
            varied++;
        }
    }

    // This deliberately rejects filler/padding blobs that matched the small
    // direct-texture header pattern by accident and showed up as coloured lines.
    return meaningful >= 2 && varied >= 2;
}

static bool directRasterLooksUseful(const std::vector<uint8_t>& bytes, size_t rasterStart, size_t rasterBytes) {
    if (rasterBytes == 0) return false;
    if (rasterStart + rasterBytes > bytes.size()) return false;

    uint8_t first = bytes[rasterStart];
    int nonPad = 0;
    int varied = 0;
    size_t sampleBytes = std::min<size_t>(rasterBytes, 4096);

    for (size_t offset = 0; offset < sampleBytes; ++offset) {
        uint8_t value = bytes[rasterStart + offset];
        if (value != 0x00 && value != 0xAA && value != 0xCC && value != 0xFF) nonPad++;
        if (value != first) varied++;
    }

    // Solid-colour textures are allowed, but pure padding is not.
    return nonPad > 0 || varied > 0;
}

static bool resolveDirectTextureLayout(
    const std::vector<uint8_t>& bytes,
    size_t addr,
    size_t baseStart,
    size_t baseEnd,
    StorylandDirectTextureLayout& outLayout
) {
    if (baseEnd > bytes.size()) baseEnd = bytes.size();
    if (addr < baseStart || addr + 16 > baseEnd || (addr & 3u) != 0) return false;
    if (readU32(bytes, addr + 0) != 0xCCCCCCCCu) return false;

    uint16_t widthHalf = readU16(bytes, addr + 4);
    uint16_t formatFlags = readU16(bytes, addr + 6);
    uint32_t dataPointer = readU32(bytes, addr + 8);
    uint32_t rasterFlags = readU32(bytes, addr + 12);

    uint16_t lowFlag = formatFlags & 0x00FFu;
    uint16_t highFlag = formatFlags & 0xFF00u;
    if (lowFlag != 0x25u && lowFlag != 0x45u) return false;
    if (highFlag != 0xC000u && highFlag != 0xCF00u) return false;

    int logw = int(rasterFlags & 0x3Fu);
    int logh = int((rasterFlags >> 6) & 0x3Fu);
    int depth = int((rasterFlags >> 12) & 0x3Fu);
    int mipmaps = int((rasterFlags >> 20) & 0x0Fu);
    int swizzleMask = int((rasterFlags >> 24) & 0xFFu);

    if (logw < 1 || logh < 1 || logw > 12 || logh > 12) return false;
    if (depth != 4 && depth != 8 && depth != 32) return false;
    if (mipmaps > 8) return false;

    int width = 1 << logw;
    int height = 1 << logh;
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) return false;
    if (depth == 4 && widthHalf != 0 && width != int(widthHalf) && width != int(widthHalf) * 2) return false;

    size_t pixelCount = size_t(width) * size_t(height);
    size_t rasterBytes = depth == 4 ? (pixelCount + 1) / 2 : depth == 8 ? pixelCount : pixelCount * 4;
    size_t paletteBytes = depth == 4 ? 64 : depth == 8 ? 1024 : 0;

    std::vector<size_t> candidates;
    auto addCandidate = [&](size_t value) {
        if (std::find(candidates.begin(), candidates.end(), value) == candidates.end()) candidates.push_back(value);
    };

    if (dataPointer != 0) {
        // AERA stores this pointer relative to the AERA chunk base.  Master LVZ
        // direct textures usually store an absolute decompressed-LVZ offset.
        addCandidate(baseStart + size_t(dataPointer));
        addCandidate(size_t(dataPointer));
    }
    addCandidate(addr + 16);

    for (size_t dataStart : candidates) {
        if (dataStart < addr + 16) continue;
        if (dataStart < baseStart || dataStart + rasterBytes + paletteBytes > baseEnd) continue;
        if (!directRasterLooksUseful(bytes, dataStart, rasterBytes)) continue;
        if (!directPaletteLooksUseful(bytes, dataStart + rasterBytes, paletteBytes)) continue;

        outLayout.dataStart = dataStart;
        outLayout.rasterBytes = rasterBytes;
        outLayout.paletteBytes = paletteBytes;
        outLayout.width = width;
        outLayout.height = height;
        outLayout.bpp = depth;
        outLayout.swizzled = (swizzleMask & 1) != 0;
        return true;
    }

    return false;
}

static bool looksLikeDirectLvzTextureCandidate(
    const std::vector<uint8_t>& bytes,
    size_t addr,
    size_t baseStart,
    size_t baseEnd
) {
    StorylandDirectTextureLayout layout;
    return resolveDirectTextureLayout(bytes, addr, baseStart, baseEnd, layout);
}

static void writeDirectTexturePixelFlipped(
    StorylandDirectTextureResource& outTexture,
    int x,
    int y,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a
) {
    int width = outTexture.width;
    int height = outTexture.height;
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    size_t dst = size_t(height - 1 - y) * size_t(width) + size_t(x);
    outTexture.rgba[dst * 4 + 0] = r;
    outTexture.rgba[dst * 4 + 1] = g;
    outTexture.rgba[dst * 4 + 2] = b;
    outTexture.rgba[dst * 4 + 3] = a;
}

static bool decodeDirectTexture(
    const std::vector<uint8_t>& bytes,
    size_t addr,
    size_t baseStart,
    size_t baseEnd,
    StorylandDirectTextureResource& outTexture
) {
    StorylandDirectTextureLayout layout;
    if (!resolveDirectTextureLayout(bytes, addr, baseStart, baseEnd, layout)) return false;

    uint16_t widthHalf = readU16(bytes, addr + 4);
    uint16_t formatFlags = readU16(bytes, addr + 6);
    uint32_t rasterFlags = readU32(bytes, addr + 12);

    outTexture.headerOffset = uint32_t(addr);
    outTexture.dataOffset = uint32_t(layout.dataStart);
    outTexture.rasterFlags = rasterFlags;
    outTexture.widthHalf = widthHalf;
    outTexture.formatFlags = formatFlags;
    outTexture.width = layout.width;
    outTexture.height = layout.height;
    outTexture.bpp = layout.bpp;
    outTexture.rgba.assign(size_t(layout.width) * size_t(layout.height) * 4u, 255);

    size_t pixelCount = size_t(layout.width) * size_t(layout.height);
    size_t dataStart = layout.dataStart;

    if (layout.bpp == 4) {
        size_t paletteStart = dataStart + layout.rasterBytes;
        if (paletteStart + 64 > baseEnd) return false;

        std::vector<uint8_t> packed(bytes.begin() + dataStart, bytes.begin() + dataStart + layout.rasterBytes);
        std::vector<uint8_t> indices = directTextureExpandNibblesLoFirst(packed.data(), packed.size(), pixelCount);
        if (layout.swizzled) indices = directTextureUnswizzlePs2Indices(indices, layout.width, layout.height);

        for (int y = 0; y < layout.height; ++y) {
            for (int x = 0; x < layout.width; ++x) {
                size_t i = size_t(y) * size_t(layout.width) + size_t(x);
                size_t p = size_t(indices[i] & 0x0Fu);
                size_t pal = paletteStart + p * 4;
                writeDirectTexturePixelFlipped(
                    outTexture,
                    x,
                    y,
                    bytes[pal + 0],
                    bytes[pal + 1],
                    bytes[pal + 2],
                    uint8_t(std::min(255, int(bytes[pal + 3]) * 255 / 128))
                );
            }
        }
        return true;
    }

    if (layout.bpp == 8) {
        size_t paletteStart = dataStart + layout.rasterBytes;
        if (paletteStart + 1024 > baseEnd) return false;

        std::vector<uint8_t> indices(bytes.begin() + dataStart, bytes.begin() + dataStart + layout.rasterBytes);
        if (layout.swizzled) indices = directTextureUnswizzlePs2Indices(indices, layout.width, layout.height);
        for (uint8_t& value : indices) {
            static const uint8_t mapping[4] = {0x00, 0x10, 0x08, 0x18};
            value = uint8_t((value & uint8_t(~0x18)) | mapping[(value & 0x18) >> 3]);
        }

        for (int y = 0; y < layout.height; ++y) {
            for (int x = 0; x < layout.width; ++x) {
                size_t i = size_t(y) * size_t(layout.width) + size_t(x);
                size_t p = indices[i];
                size_t pal = paletteStart + p * 4;
                writeDirectTexturePixelFlipped(
                    outTexture,
                    x,
                    y,
                    bytes[pal + 0],
                    bytes[pal + 1],
                    bytes[pal + 2],
                    uint8_t(std::min(255, int(bytes[pal + 3]) * 255 / 128))
                );
            }
        }
        return true;
    }

    if (layout.bpp == 32) {
        if (dataStart + layout.rasterBytes > baseEnd) return false;
        for (int y = 0; y < layout.height; ++y) {
            for (int x = 0; x < layout.width; ++x) {
                size_t src = dataStart + (size_t(y) * size_t(layout.width) + size_t(x)) * 4u;
                writeDirectTexturePixelFlipped(
                    outTexture,
                    x,
                    y,
                    bytes[src + 0],
                    bytes[src + 1],
                    bytes[src + 2],
                    uint8_t(std::min(255, int(bytes[src + 3]) * 255 / 128))
                );
            }
        }
        return true;
    }

    return false;
}

}

void StorylandArchiveBrowser::clear() {
    currentImgPath.clear();
    currentLvzPath.clear();
    currentImgSize = 0;
    currentLevelSummary.clear();
    currentLvzBytes.clear();
    currentImgBytes.clear();
    archiveEntries.clear();
    worldPlacements.clear();
    worldSectors.clear();
    worldMeshCache.clear();
    directTextureCache.clear();
}

bool StorylandArchiveBrowser::readWholeFile(const std::wstring& path, std::vector<uint8_t>& outBytes, std::string& errorMessage) const {
    FILE* file = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr) {
        errorMessage = "Could not open file.";
        return false;
    }
#else
    file = fopen(std::string(path.begin(), path.end()).c_str(), "rb");
    if (file == nullptr) {
        errorMessage = "Could not open file.";
        return false;
    }
#endif

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        errorMessage = "Could not seek file.";
        return false;
    }

    long fileSize = ftell(file);
    if (fileSize < 0) {
        fclose(file);
        errorMessage = "Could not read file size.";
        return false;
    }

    rewind(file);
    outBytes.resize(size_t(fileSize));
    if (!outBytes.empty()) {
        size_t readCount = fread(outBytes.data(), 1, outBytes.size(), file);
        if (readCount != outBytes.size()) {
            fclose(file);
            errorMessage = "Could not read complete file.";
            return false;
        }
    }

    fclose(file);
    return true;
}


bool StorylandArchiveBrowser::writeWholeFile(const std::wstring& path, const std::vector<uint8_t>& bytes, std::string& errorMessage) const {
    if (path.empty()) {
        errorMessage = "Output path is empty.";
        return false;
    }

    std::error_code fsError;
    std::filesystem::path outputPath(path);
    std::filesystem::path parentPath = outputPath.parent_path();
    if (!parentPath.empty() && !std::filesystem::exists(parentPath, fsError)) {
        fsError.clear();
        if (!std::filesystem::create_directories(parentPath, fsError) && fsError) {
            errorMessage =
                "Could not create output directory:\r\n" +
                narrowForMessage(parentPath.wstring()) +
                "\r\nReason: " + fsError.message();
            return false;
        }
    }

#ifdef _WIN32
    DWORD existingAttributes = GetFileAttributesW(path.c_str());
    if (existingAttributes != INVALID_FILE_ATTRIBUTES && (existingAttributes & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(path.c_str(), existingAttributes & ~FILE_ATTRIBUTE_READONLY);
    }

    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file == INVALID_HANDLE_VALUE) {
        DWORD errorCode = GetLastError();
        errorMessage =
            "Could not open output file:\r\n" +
            narrowForMessage(path) +
            "\r\nReason: " + windowsErrorMessage(errorCode) +
            "\r\n\r\nMost common causes: the file is open/locked by another program, the folder is read-only/protected, "
            "or the current output path is invalid.";
        return false;
    }

    uint64_t writtenTotal = 0;
    while (writtenTotal < bytes.size()) {
        DWORD chunkSize = DWORD(std::min<uint64_t>(bytes.size() - writtenTotal, 64ull * 1024ull * 1024ull));
        DWORD writtenNow = 0;
        BOOL ok = WriteFile(
            file,
            bytes.data() + writtenTotal,
            chunkSize,
            &writtenNow,
            nullptr
        );

        if (!ok || writtenNow != chunkSize) {
            DWORD errorCode = GetLastError();
            CloseHandle(file);
            errorMessage =
                "Could not write complete output file:\r\n" +
                narrowForMessage(path) +
                "\r\nWritten bytes: " + std::to_string(writtenTotal + writtenNow) +
                " / " + std::to_string(bytes.size()) +
                "\r\nReason: " + windowsErrorMessage(errorCode);
            return false;
        }

        writtenTotal += writtenNow;
    }

    if (!FlushFileBuffers(file)) {
        DWORD errorCode = GetLastError();
        CloseHandle(file);
        errorMessage =
            "Could not flush output file:\r\n" +
            narrowForMessage(path) +
            "\r\nReason: " + windowsErrorMessage(errorCode);
        return false;
    }

    CloseHandle(file);
    return true;
#else
    FILE* file = fopen(std::string(path.begin(), path.end()).c_str(), "wb");
    if (file == nullptr) {
        errorMessage =
            "Could not open output file:\r\n" +
            narrowForMessage(path) +
            "\r\nReason: " + std::strerror(errno);
        return false;
    }

    if (!bytes.empty()) {
        size_t written = fwrite(bytes.data(), 1, bytes.size(), file);
        if (written != bytes.size()) {
            fclose(file);
            errorMessage =
                "Could not write complete output file:\r\n" +
                narrowForMessage(path) +
                "\r\nWritten bytes: " + std::to_string(written) +
                " / " + std::to_string(bytes.size()) +
                "\r\nReason: " + std::strerror(errno);
            return false;
        }
    }

    fclose(file);
    return true;
#endif
}


bool StorylandArchiveBrowser::inflateLvzBytes(const std::vector<uint8_t>& packed, std::vector<uint8_t>& unpacked, std::string& errorMessage) const {
    unpacked.clear();

    z_stream stream = {};
    int status = inflateInit(&stream);
    if (status != Z_OK) {
        errorMessage = "zlib inflateInit failed for LVZ.";
        return false;
    }

    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(packed.data()));
    stream.avail_in = static_cast<uInt>(packed.size());

    std::array<uint8_t, 65536> temp {};
    for (;;) {
        stream.next_out = reinterpret_cast<Bytef*>(temp.data());
        stream.avail_out = static_cast<uInt>(temp.size());

        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            errorMessage = "Could not inflate LVZ zlib stream.";
            return false;
        }

        size_t produced = temp.size() - stream.avail_out;
        unpacked.insert(unpacked.end(), temp.data(), temp.data() + produced);

        if (status == Z_STREAM_END) break;
        if (produced == 0 && stream.avail_in == 0) {
            inflateEnd(&stream);
            errorMessage = "LVZ inflate stopped before stream end.";
            return false;
        }
    }

    inflateEnd(&stream);
    if (unpacked.empty()) {
        errorMessage = "LVZ inflated to an empty buffer.";
        return false;
    }
    return true;
}

bool StorylandArchiveBrowser::deflateLvzBytes(const std::vector<uint8_t>& unpacked, std::vector<uint8_t>& packed, std::string& errorMessage) const {
    packed.clear();

    z_stream stream = {};
    int status = deflateInit(&stream, Z_BEST_COMPRESSION);
    if (status != Z_OK) {
        errorMessage = "zlib deflateInit failed for LVZ.";
        return false;
    }

    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(unpacked.data()));
    stream.avail_in = static_cast<uInt>(unpacked.size());

    std::array<uint8_t, 65536> temp {};
    for (;;) {
        stream.next_out = reinterpret_cast<Bytef*>(temp.data());
        stream.avail_out = static_cast<uInt>(temp.size());

        int flush = stream.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH;
        status = deflate(&stream, flush);
        if (status != Z_OK && status != Z_STREAM_END) {
            deflateEnd(&stream);
            errorMessage = "Could not deflate LVZ zlib stream.";
            return false;
        }

        size_t produced = temp.size() - stream.avail_out;
        packed.insert(packed.end(), temp.data(), temp.data() + produced);

        if (status == Z_STREAM_END) break;
        if (produced == 0 && stream.avail_in == 0) {
            deflateEnd(&stream);
            errorMessage = "LVZ deflate stopped before stream end.";
            return false;
        }
    }

    deflateEnd(&stream);
    if (packed.empty()) {
        errorMessage = "LVZ deflated to an empty buffer.";
        return false;
    }
    return true;
}


bool StorylandArchiveBrowser::autoFindCompanionImgForLvz(const std::wstring& lvzPath, std::wstring& outImgPath) const {
    std::error_code ec;
    std::filesystem::path lvz(lvzPath);
    std::filesystem::path folder = lvz.parent_path();
    std::wstring stem = lvz.stem().wstring();

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(folder / (stem + L".img"));
    candidates.push_back(folder / (stem + L".IMG"));

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            outImgPath = candidate.wstring();
            return true;
        }
    }

    for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec)) continue;
        if (getExtensionLower(item.path().wstring()) == L".img") {
            if (lowerWide(item.path().stem().wstring()) == lowerWide(stem)) {
                outImgPath = item.path().wstring();
                return true;
            }
        }
    }

    return false;
}

bool StorylandArchiveBrowser::autoFindCompanionLvzForImg(const std::wstring& imgPath, std::wstring& outLvzPath) const {
    std::error_code ec;
    std::filesystem::path img(imgPath);
    std::filesystem::path folder = img.parent_path();
    std::wstring stem = img.stem().wstring();

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(folder / (stem + L".lvz"));
    candidates.push_back(folder / (stem + L".LVZ"));

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            outLvzPath = candidate.wstring();
            return true;
        }
    }

    for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec)) continue;
        if (getExtensionLower(item.path().wstring()) == L".lvz") {
            if (lowerWide(item.path().stem().wstring()) == lowerWide(stem)) {
                outLvzPath = item.path().wstring();
                return true;
            }
        }
    }

    return false;
}


void StorylandArchiveBrowser::buildDirectTexturesFromLvz() {
    directTextureCache.clear();

    std::set<uint64_t> seenHeaders;

    auto appendTexture = [&](const std::vector<uint8_t>& bytes, size_t offset, size_t baseStart, size_t baseEnd, const char* prefix) {
        uint64_t key = (uint64_t(baseStart & 0xFFFFFFFFu) << 32) | uint64_t(offset & 0xFFFFFFFFu);
        if (seenHeaders.find(key) != seenHeaders.end()) return;

        StorylandDirectTextureResource texture;
        if (!decodeDirectTexture(bytes, offset, baseStart, baseEnd, texture)) return;

        texture.index = uint32_t(directTextureCache.size());
        char name[128] = {};
        std::snprintf(
            name,
            sizeof(name),
            "%s_%04u_%dx%d_%dbpp",
            prefix,
            texture.index,
            texture.width,
            texture.height,
            texture.bpp
        );
        texture.name = name;

        directTextureCache.push_back(std::move(texture));
        seenHeaders.insert(key);
    };

    for (size_t offset = 0x40; offset + 80 <= currentLvzBytes.size(); offset += 4) {
        if (readU32(currentLvzBytes, offset) != 0xCCCCCCCCu) continue;
        appendTexture(currentLvzBytes, offset, 0, currentLvzBytes.size(), "lvz_direct_texture");
        if (directTextureCache.size() > 8192) break;
    }

    // VCS AREA chunks are stored in the IMG as AERA headers referenced by a
    // master-WRLD table.  Their texture blobs use the same 0xCCCCCCCC direct
    // texture header, but the data pointer is relative to the AERA chunk base.
    for (const StorylandArchiveEntry& entry : archiveEntries) {
        if (!isAreaIdent(entry.chunkIdent)) continue;
        size_t baseStart = size_t(entry.byteOffset);
        size_t baseEnd = size_t(std::min<uint64_t>(currentImgBytes.size(), entry.byteOffset + entry.byteSize));
        if (baseStart >= currentImgBytes.size() || baseStart + 0x20 > baseEnd) continue;

        char prefix[64] = {};
        std::snprintf(prefix, sizeof(prefix), "area_%04u_direct_texture", entry.index);

        for (size_t offset = baseStart + 0x20; offset + 80 <= baseEnd; offset += 4) {
            if (readU32(currentImgBytes, offset) != 0xCCCCCCCCu) continue;
            appendTexture(currentImgBytes, offset, baseStart, baseEnd, prefix);
            if (directTextureCache.size() > 16384) break;
        }
    }
}

void StorylandArchiveBrowser::buildWorldSectorsAndPlacements() {
    worldSectors.clear();
    worldPlacements.clear();

    std::vector<std::pair<uint32_t, uint32_t>> rows;
    if (!readSectorRowsFromLvz(currentLvzBytes, rows)) return;

    int rowCount = detectWorldGameRowCount(rows.size());
    // Do not clamp sGeomInstance.resId to the archive browser entry count here.
    // The WRLD Resource[] table index space is not the same thing as the sorted
    // browser entry index space, and clamping it made valid map rows disappear.
    uint32_t maxResourceId = 0;

    std::set<uint64_t> seenPlacements;
    uint32_t sectorIndex = 0;

    for (size_t rowIndex = 0; rowIndex + 1 < rows.size(); ++rowIndex) {
        uint32_t firstHeader = rows[rowIndex].first;
        uint32_t nextHeader = rows[rowIndex + 1].first;
        if (nextHeader <= firstHeader) continue;

        uint32_t startX = rows[rowIndex].second;
        uint32_t headerCount = (nextHeader - firstHeader) / 0x20u;
        if (headerCount == 0 || headerCount > 256) continue;

        for (uint32_t headerIndex = 0; headerIndex < headerCount; ++headerIndex) {
            uint32_t headerAddress = firstHeader + headerIndex * 0x20u;
            if (headerAddress + 0x20 > currentLvzBytes.size()) break;
            if (!isWorldLikeIdent(readU32(currentLvzBytes, headerAddress))) break;

            uint32_t fileSize = readU32(currentLvzBytes, headerAddress + 0x08);
            uint32_t globalTab = readU32(currentLvzBytes, headerAddress + 0x18);
            if (fileSize < 0x20) continue;
            if (globalTab >= currentImgBytes.size()) continue;

            uint64_t sectorEnd = uint64_t(globalTab) + uint64_t(fileSize - 0x20u);
            if (sectorEnd > currentImgBytes.size()) sectorEnd = currentImgBytes.size();
            if (sectorEnd <= globalTab + 0x30u) continue;

            uint32_t sectorX = startX + headerIndex;
            uint32_t sectorY = uint32_t(rowIndex);

            float originX = 0.0f;
            float originY = 0.0f;
            float originZ = 0.0f;
            sectorOriginForXY(rowCount, sectorX, sectorY, originX, originY, originZ);

            StorylandWorldSector sector;
            sector.sectorIndex = sectorIndex;
            sector.sectorX = sectorX;
            sector.sectorY = sectorY;
            sector.headerOffset = headerAddress;
            sector.imgOffset = globalTab;
            sector.byteSize = uint64_t(fileSize - 0x20u);
            sector.originX = originX;
            sector.originY = originY;
            sector.originZ = originZ;
            worldSectors.push_back(sector);

            int passCount = rowCount == 47 ? 8 : 9;
            if (globalTab + 0x08u + uint32_t(passCount) * 4u > currentImgBytes.size()) {
                sectorIndex++;
                continue;
            }

            std::vector<uint32_t> passPointers;
            passPointers.reserve(size_t(passCount));
            bool validPointers = true;
            for (int passIndex = 0; passIndex < passCount; ++passIndex) {
                uint32_t pointer = readU32(currentImgBytes, globalTab + 0x08u + uint32_t(passIndex) * 4u);
                if (pointer < 0x20 || (pointer & 3u) != 0) validPointers = false;
                if (!passPointers.empty() && pointer < passPointers.back()) validPointers = false;
                if (uint64_t(globalTab) + uint64_t(pointer) - 0x20ull > sectorEnd) validPointers = false;
                passPointers.push_back(pointer);
            }

            if (!validPointers) {
                sectorIndex++;
                continue;
            }

            for (int passIndex = 0; passIndex + 1 < passCount; ++passIndex) {
                uint32_t startPointer = passPointers[size_t(passIndex)];
                uint32_t stopPointer = passPointers[size_t(passIndex + 1)];
                if (stopPointer <= startPointer) continue;

                uint64_t spanStart64 = std::max<uint64_t>(globalTab, uint64_t(globalTab) + uint64_t(startPointer) - 0x20ull);
                uint64_t spanStop64 = std::min<uint64_t>(sectorEnd, uint64_t(globalTab) + uint64_t(stopPointer) - 0x20ull);
                if (spanStop64 <= spanStart64) continue;

                const char* passName = passNameForIndex(rowCount, size_t(passIndex));
                if (passIsLod(passName)) continue;

                uint64_t rowOffset64 = spanStart64;
                while (rowOffset64 + 0x50ull <= spanStop64) {
                    size_t rowOffset = size_t(rowOffset64);
                    if (!looksLikeImgInstanceRow(currentImgBytes, rowOffset, maxResourceId)) {
                        rowOffset64 += 0x50ull;
                        continue;
                    }

                    uint16_t iplRaw = readU16(currentImgBytes, rowOffset + 0x00);
                    uint16_t resourceId = readU16(currentImgBytes, rowOffset + 0x02);
                    uint32_t iplId = uint32_t(iplRaw & 0x7FFFu);

                    uint64_t dedupeKey =
                        (uint64_t(iplId) << 32) |
                        (uint64_t(resourceId) << 16) |
                        uint64_t(passIndex & 0xFFFF);
                    if (seenPlacements.find(dedupeKey) != seenPlacements.end()) {
                        rowOffset64 += 0x50ull;
                        continue;
                    }
                    seenPlacements.insert(dedupeKey);

                    float boundX = halfToFloat(readU16(currentImgBytes, rowOffset + 0x04));
                    float boundY = halfToFloat(readU16(currentImgBytes, rowOffset + 0x06));
                    float boundZ = halfToFloat(readU16(currentImgBytes, rowOffset + 0x08));
                    float boundR = halfToFloat(readU16(currentImgBytes, rowOffset + 0x0A));

                    float rightX = readF32(currentImgBytes, rowOffset + 0x10 + 0x00);
                    float rightY = readF32(currentImgBytes, rowOffset + 0x10 + 0x04);
                    float rightZ = readF32(currentImgBytes, rowOffset + 0x10 + 0x08);
                    float upX = readF32(currentImgBytes, rowOffset + 0x10 + 0x10);
                    float upY = readF32(currentImgBytes, rowOffset + 0x10 + 0x14);
                    float upZ = readF32(currentImgBytes, rowOffset + 0x10 + 0x18);
                    float atX = readF32(currentImgBytes, rowOffset + 0x10 + 0x20);
                    float atY = readF32(currentImgBytes, rowOffset + 0x10 + 0x24);
                    float atZ = readF32(currentImgBytes, rowOffset + 0x10 + 0x28);

                    float posX = readF32(currentImgBytes, rowOffset + 0x10 + 0x30) + originX;
                    float posY = readF32(currentImgBytes, rowOffset + 0x10 + 0x34) + originY;
                    float posZ = readF32(currentImgBytes, rowOffset + 0x10 + 0x38) + originZ;

                    if (!finiteReasonable(posX, 1000000.0f) ||
                        !finiteReasonable(posY, 1000000.0f) ||
                        !finiteReasonable(posZ, 1000000.0f)) {
                        rowOffset64 += 0x50ull;
                        continue;
                    }

                    StorylandWorldPlacement placement;
                    placement.resourceIndex = resourceId;
                    placement.iplId = iplId;
                    placement.iplRaw = iplRaw;
                    placement.sectorIndex = sectorIndex;
                    placement.sectorX = sectorX;
                    placement.sectorY = sectorY;
                    placement.passIndex = uint32_t(passIndex);
                    placement.imgOffset = rowOffset64;
                    placement.x = posX;
                    placement.y = posY;
                    placement.z = posZ;
                    placement.boundX = boundX;
                    placement.boundY = boundY;
                    placement.boundZ = boundZ;
                    placement.boundRadius = std::isfinite(boundR) && boundR > 0.01f ? boundR : 1.0f;
                    placement.scaleX = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
                    placement.scaleY = std::sqrt(upX * upX + upY * upY + upZ * upZ);
                    placement.scaleZ = std::sqrt(atX * atX + atY * atY + atZ * atZ);
                    for (int matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
                        placement.matrix[matrixIndex] = readF32(currentImgBytes, rowOffset + 0x10 + size_t(matrixIndex) * 4u);
                    }
                    placement.matrix[12] += originX;
                    placement.matrix[13] += originY;
                    placement.matrix[14] += originZ;
                    placement.passName = passName;
                    worldPlacements.push_back(placement);

                    rowOffset64 += 0x50ull;
                }
            }

            sectorIndex++;
        }
    }
}


void StorylandArchiveBrowser::buildWorldMeshes() {
    worldMeshCache.clear();
    if (worldSectors.empty() || worldPlacements.empty() || currentImgBytes.empty()) return;

    std::set<uint64_t> neededKeys;
    for (const StorylandWorldPlacement& placement : worldPlacements) {
        uint64_t key = (uint64_t(placement.sectorIndex) << 32) | uint64_t(placement.resourceIndex);
        neededKeys.insert(key);
    }
    if (neededKeys.empty()) return;

    std::set<uint64_t> parsedKeys;
    for (const StorylandWorldSector& sector : worldSectors) {
        uint64_t cont = sector.imgOffset;
        uint64_t end = std::min<uint64_t>(currentImgBytes.size(), sector.imgOffset + sector.byteSize);
        if (cont + 8 > end) continue;

        uint32_t resourcesPointer = readU32(currentImgBytes, size_t(cont) + 0x00);
        uint32_t resourceCount = readU16(currentImgBytes, size_t(cont) + 0x04);
        if (resourceCount == 0 || resourceCount > 4096) continue;

        uint64_t listStart = cont + uint64_t(resourcesPointer) - 0x20ull;
        if (listStart < cont || listStart + uint64_t(resourceCount) * 8ull > end) continue;

        for (uint32_t resourceIndex = 0; resourceIndex < resourceCount; ++resourceIndex) {
            size_t rowOffset = size_t(listStart + uint64_t(resourceIndex) * 8ull);
            int32_t signedResourceId = readI32(currentImgBytes, rowOffset + 0x00);
            if (signedResourceId < 0) continue;
            uint32_t resourceId = uint32_t(signedResourceId);

            uint64_t key = (uint64_t(sector.sectorIndex) << 32) | uint64_t(resourceId);
            if (neededKeys.find(key) == neededKeys.end()) continue;
            if (parsedKeys.find(key) != parsedKeys.end()) continue;

            uint32_t rawPointer = readU32(currentImgBytes, rowOffset + 0x04);
            uint64_t rawOffset = cont + uint64_t(rawPointer) - 0x20ull;
            if (rawOffset < cont || rawOffset + 4 > end) continue;

            StorylandWorldMesh mesh;
            if (!parseWorldOverlayMesh(currentImgBytes, size_t(rawOffset), size_t(end), sector.sectorIndex, resourceId, mesh)) continue;
            if (mesh.vertices.empty() || mesh.triangles.empty()) continue;

            worldMeshCache.push_back(std::move(mesh));
            parsedKeys.insert(key);
        }
    }
}

bool StorylandArchiveBrowser::buildEntriesFromLvzAndImg(std::string& errorMessage) {
    archiveEntries.clear();

    if (currentLvzBytes.size() < 0x20) {
        errorMessage = "Inflated LVZ data is too small.";
        return false;
    }

    if (currentImgSize == 0) {
        errorMessage = "Companion IMG is empty or missing.";
        return false;
    }

    uint32_t topIdent = readU32(currentLvzBytes, 0);
    uint32_t topFileSize = readU32(currentLvzBytes, 8);
    uint32_t topDataSize = readU32(currentLvzBytes, 12);

    uint32_t worldCount = 0;
    uint32_t areaCount = 0;
    uint32_t mdlCount = 0;
    uint32_t texCount = 0;
    uint32_t otherCount = 0;
    uint32_t scannedAreaHeaderCount = 0;

    for (size_t offset = 0; offset + 0x20 <= currentLvzBytes.size(); offset += 4) {
        if (isAreaIdent(readU32(currentLvzBytes, offset))) scannedAreaHeaderCount++;
        uint32_t ident = readU32(currentLvzBytes, offset + 0);
        if (!knownChunkIdent(ident)) continue;

        uint32_t fileSize = readU32(currentLvzBytes, offset + 8);
        uint32_t globalTab = readU32(currentLvzBytes, offset + 0x18);

        if (fileSize < 0x20 || fileSize > 0x08000000) continue;
        if (globalTab >= currentImgSize) continue;
        if (uint64_t(globalTab) + uint64_t(fileSize - 0x20) > currentImgSize) continue;

        StorylandArchiveEntry entry;
        entry.index = uint32_t(archiveEntries.size());
        entry.startSector = globalTab / 2048u;
        entry.sectorCount = (fileSize + 2047u) / 2048u;
        entry.byteOffset = globalTab;
        entry.byteSize = fileSize - 0x20u;
        entry.lvzHeaderOffset = uint32_t(offset);
        entry.chunkIdent = ident;
        entry.usesLvzChunkHeader = true;

        uint32_t typeIndex = 0;
        if (ident == WRLD_IDENT) typeIndex = worldCount++;
        else if (isAreaIdent(ident)) typeIndex = areaCount++;
        else if (ident == MDL_IDENT) typeIndex = mdlCount++;
        else if (ident == TEX_IDENT) typeIndex = texCount++;
        else typeIndex = otherCount++;

        char name[96] = {};
        std::snprintf(name, sizeof(name), "%s_%04u%s", labelForIdent(ident), typeIndex, extensionForIdent(ident).c_str());
        entry.name = name;

        bool duplicate = false;
        for (const auto& existing : archiveEntries) {
            if (existing.byteOffset == entry.byteOffset &&
                existing.byteSize == entry.byteSize &&
                existing.chunkIdent == entry.chunkIdent) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) archiveEntries.push_back(entry);
    }

    // VCS AREA is not stored as ASCII "AREA" records in the inflated LVZ.
    // The master WRLD has count/table pairs that point to 16-byte descriptors:
    //   u32 area_id, u32 img_offset, u32 file_size, u32 unknown/resource_count
    // The actual chunk in IMG starts with "AERA" and contains models/textures.
    uint32_t areaDescriptorTableCount = 0;
    uint32_t areaDescriptorEntryCount = 0;
    for (size_t meta = 0x20; meta + 8 <= std::min<size_t>(currentLvzBytes.size(), 0x380); meta += 4) {
        uint32_t descriptorCount = readU32(currentLvzBytes, meta + 0);
        uint32_t descriptorTable = readU32(currentLvzBytes, meta + 4);
        if (descriptorCount == 0 || descriptorCount > 4096) continue;
        if (descriptorTable < 0x40 || descriptorTable + uint64_t(descriptorCount) * 16ull > currentLvzBytes.size()) continue;

        uint32_t validRows = 0;
        uint32_t sampleRows = std::min<uint32_t>(descriptorCount, 16u);
        for (uint32_t i = 0; i < sampleRows; ++i) {
            size_t row = size_t(descriptorTable) + size_t(i) * 16u;
            uint32_t imgOffset = readU32(currentLvzBytes, row + 4);
            uint32_t fileSize = readU32(currentLvzBytes, row + 8);
            if (fileSize < 0x20 || fileSize > 0x02000000) continue;
            if (uint64_t(imgOffset) + uint64_t(fileSize) > currentImgBytes.size()) continue;
            if (isAreaIdent(readU32(currentImgBytes, imgOffset))) validRows++;
        }

        if (validRows == 0) continue;
        if (descriptorCount > 3 && validRows < std::min<uint32_t>(sampleRows, 3u)) continue;

        areaDescriptorTableCount++;
        for (uint32_t i = 0; i < descriptorCount; ++i) {
            size_t row = size_t(descriptorTable) + size_t(i) * 16u;
            uint32_t areaId = readU32(currentLvzBytes, row + 0);
            uint32_t imgOffset = readU32(currentLvzBytes, row + 4);
            uint32_t tableFileSize = readU32(currentLvzBytes, row + 8);
            uint32_t areaIdent = imgOffset + 4 <= currentImgBytes.size() ? readU32(currentImgBytes, imgOffset) : 0;
            if (!isAreaIdent(areaIdent)) continue;

            uint32_t headerFileSize = readU32(currentImgBytes, size_t(imgOffset) + 8);
            uint32_t fileSize = headerFileSize >= 0x20 && headerFileSize <= tableFileSize + 0x1000 ? headerFileSize : tableFileSize;
            if (fileSize < 0x20 || uint64_t(imgOffset) + uint64_t(fileSize) > currentImgBytes.size()) continue;

            bool duplicate = false;
            for (const auto& existing : archiveEntries) {
                if (existing.byteOffset == imgOffset && existing.chunkIdent == areaIdent) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

            StorylandArchiveEntry entry;
            entry.index = uint32_t(archiveEntries.size());
            entry.startSector = imgOffset / 2048u;
            entry.sectorCount = (fileSize + 2047u) / 2048u;
            entry.byteOffset = imgOffset;
            entry.byteSize = fileSize;
            entry.lvzHeaderOffset = uint32_t(row);
            entry.chunkIdent = areaIdent;
            entry.usesLvzChunkHeader = false;

            char name[128] = {};
            std::snprintf(name, sizeof(name), "area_%04u_id_%08X.area", areaCount++, areaId);
            entry.name = name;
            archiveEntries.push_back(entry);
            areaDescriptorEntryCount++;
        }
    }

    if (archiveEntries.empty()) {
        errorMessage = "No LVZ/IMG world, AREA, model, or texture entries were found.";
        return false;
    }

    std::sort(archiveEntries.begin(), archiveEntries.end(), [](const StorylandArchiveEntry& a, const StorylandArchiveEntry& b) {
        if (a.byteOffset != b.byteOffset) return a.byteOffset < b.byteOffset;
        return a.lvzHeaderOffset < b.lvzHeaderOffset;
    });

    for (size_t i = 0; i < archiveEntries.size(); ++i) archiveEntries[i].index = uint32_t(i);

    currentLevelSummary =
        "Retail LVZ+IMG browse: no .DIR used. Entries are reconstructed from LVZ sChunkHeader records; IMG stores each payload after the 0x20-byte chunk header.";
    currentLevelSummary += " LVZ topIdent=" + std::string(literalForIdent(topIdent));
    currentLevelSummary += " areaHeaders=" + std::to_string(scannedAreaHeaderCount);
    currentLevelSummary += " areaDescriptorTables=" + std::to_string(areaDescriptorTableCount);
    currentLevelSummary += " areaEntries=" + std::to_string(areaCount);
    currentLevelSummary += " areaImgEntries=" + std::to_string(areaDescriptorEntryCount);
    currentLevelSummary += " LVZ topIdentRaw=0x" + std::to_string(topIdent);
    currentLevelSummary += " fileSize=" + std::to_string(topFileSize);
    currentLevelSummary += " dataSize=" + std::to_string(topDataSize);
    return true;
}

bool StorylandArchiveBrowser::loadLvzWithCompanionImg(const std::wstring& lvzPath, std::string& errorMessage) {
    clear();

    std::vector<uint8_t> packedLvz;
    if (!readWholeFile(lvzPath, packedLvz, errorMessage)) return false;
    if (!inflateLvzBytes(packedLvz, currentLvzBytes, errorMessage)) return false;

    std::wstring imgPath;
    if (!autoFindCompanionImgForLvz(lvzPath, imgPath)) {
        errorMessage = "Could not find the companion IMG beside this LVZ. Retail LVZ+IMG does not use .DIR; place the matching .IMG beside the .LVZ.";
        return false;
    }

    std::error_code ec;
    currentImgSize = std::filesystem::file_size(std::filesystem::path(imgPath), ec);
    currentImgPath = imgPath;
    currentLvzPath = lvzPath;
    if (!readWholeFile(imgPath, currentImgBytes, errorMessage)) return false;
    currentImgSize = currentImgBytes.size();

    if (!buildEntriesFromLvzAndImg(errorMessage)) return false;
    buildDirectTexturesFromLvz();
    buildWorldSectorsAndPlacements();
    buildWorldMeshes();
    return true;
}

bool StorylandArchiveBrowser::loadImgFromFile(const std::wstring& imgPath, std::string& errorMessage) {
    std::wstring lvzPath;
    if (!autoFindCompanionLvzForImg(imgPath, lvzPath)) {
        errorMessage = "IMG cannot be browsed by .DIR in retail LCS/VCS LVZ+IMG mode. Open the matching LVZ, or put the matching same-stem LVZ beside this IMG.";
        return false;
    }

    if (!loadLvzWithCompanionImg(lvzPath, errorMessage)) return false;

    std::filesystem::path requested(imgPath);
    std::filesystem::path loaded(currentImgPath);
    if (lowerWide(requested.wstring()) != lowerWide(loaded.wstring())) {
        currentImgPath = imgPath;
    }

    return true;
}

const std::vector<StorylandArchiveEntry>& StorylandArchiveBrowser::entries() const { return archiveEntries; }
const std::vector<StorylandWorldPlacement>& StorylandArchiveBrowser::placements() const { return worldPlacements; }
const std::vector<StorylandWorldSector>& StorylandArchiveBrowser::sectors() const { return worldSectors; }
const std::vector<StorylandWorldMesh>& StorylandArchiveBrowser::worldMeshes() const { return worldMeshCache; }
const std::vector<StorylandDirectTextureResource>& StorylandArchiveBrowser::directTextures() const { return directTextureCache; }
const std::wstring& StorylandArchiveBrowser::imgPath() const { return currentImgPath; }
const std::wstring& StorylandArchiveBrowser::lvzPath() const { return currentLvzPath; }
bool StorylandArchiveBrowser::hasLvzContext() const { return !currentLvzPath.empty(); }
bool StorylandArchiveBrowser::hasImgContext() const { return !currentImgPath.empty(); }
uint64_t StorylandArchiveBrowser::imgFileSize() const { return currentImgSize; }
std::string StorylandArchiveBrowser::levelSummary() const { return currentLevelSummary; }

bool StorylandArchiveBrowser::extractEntryBytes(size_t index, std::vector<uint8_t>& outBytes, std::string& errorMessage) const {
    if (index >= archiveEntries.size()) {
        errorMessage = "Archive entry index out of range.";
        return false;
    }

    if (currentImgPath.empty()) {
        errorMessage = "No IMG file is currently open.";
        return false;
    }

    const StorylandArchiveEntry& entry = archiveEntries[index];

    FILE* file = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&file, currentImgPath.c_str(), L"rb") != 0 || file == nullptr) {
        errorMessage = "Could not open IMG file for extraction.";
        return false;
    }
#else
    file = fopen(std::string(currentImgPath.begin(), currentImgPath.end()).c_str(), "rb");
    if (file == nullptr) {
        errorMessage = "Could not open IMG file for extraction.";
        return false;
    }
#endif

    if (_fseeki64(file, static_cast<__int64>(entry.byteOffset), SEEK_SET) != 0) {
        fclose(file);
        errorMessage = "Could not seek to IMG entry.";
        return false;
    }

    std::vector<uint8_t> payload(static_cast<size_t>(entry.byteSize));
    if (!payload.empty()) {
        size_t readCount = fread(payload.data(), 1, payload.size(), file);
        if (readCount != payload.size()) {
            fclose(file);
            errorMessage = "Could not read IMG entry bytes.";
            return false;
        }
    }

    fclose(file);

    if (entry.usesLvzChunkHeader) {
        if (entry.lvzHeaderOffset + 0x20 > currentLvzBytes.size()) {
            errorMessage = "LVZ chunk header offset is invalid.";
            return false;
        }

        outBytes.resize(0x20 + payload.size());
        std::copy(
            currentLvzBytes.begin() + entry.lvzHeaderOffset,
            currentLvzBytes.begin() + entry.lvzHeaderOffset + 0x20,
            outBytes.begin()
        );

        if (!payload.empty()) std::copy(payload.begin(), payload.end(), outBytes.begin() + 0x20);
        writeU32(outBytes, 8, uint32_t(outBytes.size()));
        return true;
    }

    outBytes = std::move(payload);
    return true;
}

bool StorylandArchiveBrowser::rebuildParsedCaches(std::string& errorMessage) {
    if (!buildEntriesFromLvzAndImg(errorMessage)) return false;
    buildDirectTexturesFromLvz();
    buildWorldSectorsAndPlacements();
    buildWorldMeshes();
    currentImgSize = currentImgBytes.size();
    return true;
}

bool StorylandArchiveBrowser::replaceEntryBytes(size_t index, const std::vector<uint8_t>& replacementBytes, std::string& report, std::string& errorMessage) {
    report.clear();

    if (index >= archiveEntries.size()) {
        errorMessage = "Archive entry index out of range.";
        return false;
    }
    if (replacementBytes.empty()) {
        errorMessage = "Replacement file is empty.";
        return false;
    }
    if (currentImgBytes.empty() || currentLvzBytes.empty()) {
        errorMessage = "Open a retail LVZ+IMG pair before replacing an embedded resource.";
        return false;
    }

    StorylandArchiveEntry target = archiveEntries[index];
    if (target.byteOffset > currentImgBytes.size()) {
        errorMessage = "Target IMG offset is outside the IMG.";
        return false;
    }

    bool replacementHasChunkHeader = replacementBytes.size() >= 0x20;
    uint32_t replacementIdent = replacementHasChunkHeader ? readU32(replacementBytes, 0) : 0;
    uint32_t replacementDeclaredSize = replacementHasChunkHeader ? readU32(replacementBytes, 8) : 0;
    bool replacementIdentMatches =
        replacementIdent == target.chunkIdent ||
        (isAreaIdent(replacementIdent) && isAreaIdent(target.chunkIdent)) ||
        knownChunkIdent(replacementIdent);

    std::vector<uint8_t> newImgBytes;
    std::vector<uint8_t> newLvzHeader;

    if (target.usesLvzChunkHeader) {
        if (replacementHasChunkHeader && replacementIdentMatches) {
            newLvzHeader.assign(replacementBytes.begin(), replacementBytes.begin() + 0x20);
            newImgBytes.assign(replacementBytes.begin() + 0x20, replacementBytes.end());
        } else {
            if (target.lvzHeaderOffset + 0x20 > currentLvzBytes.size()) {
                errorMessage = "Target LVZ chunk header offset is invalid.";
                return false;
            }
            newLvzHeader.assign(currentLvzBytes.begin() + target.lvzHeaderOffset, currentLvzBytes.begin() + target.lvzHeaderOffset + 0x20);
            newImgBytes = replacementBytes;
        }

        if (newLvzHeader.size() != 0x20) {
            errorMessage = "Could not build replacement LVZ chunk header.";
            return false;
        }

        writeU32(newLvzHeader, 0x00, target.chunkIdent);
        writeU32(newLvzHeader, 0x08, uint32_t(newImgBytes.size() + 0x20));
        if (readU32(newLvzHeader, 0x0C) == 0 || readU32(newLvzHeader, 0x0C) > uint32_t(newImgBytes.size() + 0x20)) {
            writeU32(newLvzHeader, 0x0C, uint32_t(newImgBytes.size()));
        }
        writeU32(newLvzHeader, 0x18, uint32_t(target.byteOffset));
    } else {
        if (replacementHasChunkHeader && replacementIdentMatches) {
            newImgBytes = replacementBytes;
        } else {
            newImgBytes = replacementBytes;
        }

        if (isAreaIdent(target.chunkIdent) && !newImgBytes.empty() && newImgBytes.size() >= 0x20) {
            writeU32(newImgBytes, 0x00, target.chunkIdent);
            writeU32(newImgBytes, 0x08, uint32_t(newImgBytes.size()));
        }
    }

    if (newImgBytes.empty()) {
        errorMessage = "Replacement resolved to an empty IMG payload.";
        return false;
    }
    if (newImgBytes.size() > 0x08000000u) {
        errorMessage = "Replacement resource is too large.";
        return false;
    }

    uint64_t oldStart64 = target.byteOffset;
    uint64_t oldSize64 = target.byteSize;
    uint64_t oldEnd64 = oldStart64 + oldSize64;
    if (oldEnd64 > currentImgBytes.size() || oldEnd64 < oldStart64) {
        errorMessage = "Target IMG byte range is invalid.";
        return false;
    }

    int64_t delta = int64_t(newImgBytes.size()) - int64_t(oldSize64);
    if (delta != 0) {
        if (delta > 0 && currentImgBytes.size() + uint64_t(delta) > 0xFFFFFFFFull) {
            errorMessage = "Replacement would make IMG larger than 4 GiB.";
            return false;
        }
        if (delta < 0 && uint64_t(-delta) > currentImgBytes.size()) {
            errorMessage = "Replacement delta underflow.";
            return false;
        }
    }

    std::vector<StorylandArchiveEntry> oldEntries = archiveEntries;

    currentImgBytes.erase(currentImgBytes.begin() + size_t(oldStart64), currentImgBytes.begin() + size_t(oldEnd64));
    currentImgBytes.insert(currentImgBytes.begin() + size_t(oldStart64), newImgBytes.begin(), newImgBytes.end());

    for (const StorylandArchiveEntry& entry : oldEntries) {
        if (entry.index == target.index) {
            if (entry.usesLvzChunkHeader) {
                if (entry.lvzHeaderOffset + 0x20 > currentLvzBytes.size()) {
                    errorMessage = "Target LVZ header shifted outside the LVZ.";
                    return false;
                }
                std::copy(newLvzHeader.begin(), newLvzHeader.end(), currentLvzBytes.begin() + entry.lvzHeaderOffset);
                writeU32(currentLvzBytes, entry.lvzHeaderOffset + 0x08, uint32_t(newImgBytes.size() + 0x20));
                writeU32(currentLvzBytes, entry.lvzHeaderOffset + 0x18, uint32_t(oldStart64));
            } else {
                if (entry.lvzHeaderOffset + 12 <= currentLvzBytes.size()) {
                    writeU32(currentLvzBytes, entry.lvzHeaderOffset + 0x04, uint32_t(oldStart64));
                    writeU32(currentLvzBytes, entry.lvzHeaderOffset + 0x08, uint32_t(newImgBytes.size()));
                }
            }
            continue;
        }

        if (delta == 0) continue;
        if (entry.byteOffset <= oldStart64) continue;

        uint64_t shifted = uint64_t(int64_t(entry.byteOffset) + delta);
        if (shifted > 0xFFFFFFFFull) {
            errorMessage = "A shifted LVZ/IMG resource offset exceeded 32-bit range.";
            return false;
        }

        if (entry.usesLvzChunkHeader) {
            if (entry.lvzHeaderOffset + 0x1C <= currentLvzBytes.size()) {
                writeU32(currentLvzBytes, entry.lvzHeaderOffset + 0x18, uint32_t(shifted));
            }
        } else {
            if (entry.lvzHeaderOffset + 8 <= currentLvzBytes.size()) {
                writeU32(currentLvzBytes, entry.lvzHeaderOffset + 0x04, uint32_t(shifted));
            }
        }
    }

    if (currentLvzBytes.size() >= 0x0C) {
        writeU32(currentLvzBytes, 0x08, uint32_t(currentLvzBytes.size()));
    }

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        errorMessage = "Replacement was applied in memory, but the LVZ/IMG browser could not rebuild its parsed index: " + rebuildError;
        return false;
    }

    report =
        "Replaced LVZ+IMG resource '" + target.name + "'\r\n"
        "Old IMG offset: " + std::to_string(oldStart64) + "\r\n"
        "Old stored bytes: " + std::to_string(oldSize64) + "\r\n"
        "New stored bytes: " + std::to_string(newImgBytes.size()) + "\r\n"
        "Delta: " + std::to_string(delta) + "\r\n"
        "Later IMG-backed LVZ records shifted: " + std::string(delta == 0 ? "no" : "yes") + "\r\n"
        "Replacement source treated as: " + std::string(target.usesLvzChunkHeader ? "sChunkHeader+payload for LVZ header record" : "whole IMG-backed chunk/payload") + "\r\n"
        "Use File > Export/Rebuild LVZ+IMG Pair to write the edited files.";

    return true;
}

struct StorylandSectorResourceSpan {
    uint32_t sectorIndex = 0;
    uint32_t resourceId = 0;
    uint64_t sectorImgOffset = 0;
    uint64_t sectorByteSize = 0;
    uint32_t sectorHeaderOffset = 0;
    uint64_t resourceRowOffset = 0;
    uint64_t rawOffset = 0;
    uint64_t rawEnd = 0;
};

static std::vector<uint8_t> normalizeWorldMeshReplacementPayload(const std::vector<uint8_t>& replacementBytes) {
    if (replacementBytes.size() >= 0x20) {
        uint32_t ident = readU32(replacementBytes, 0x00);
        uint32_t fileSize = readU32(replacementBytes, 0x08);

        bool looksLikeLeedsChunk =
            knownChunkIdent(ident) ||
            isAreaIdent(ident) ||
            ident == 0x00746578u ||
            ident == 0x006D646Cu ||
            ident == 0x57524C44u;

        if (looksLikeLeedsChunk && fileSize >= 0x20 && fileSize <= replacementBytes.size()) {
            return std::vector<uint8_t>(replacementBytes.begin() + 0x20, replacementBytes.begin() + fileSize);
        }
    }

    return replacementBytes;
}

bool StorylandArchiveBrowser::extractWorldMeshResourceBytes(uint32_t resourceId, std::vector<uint8_t>& outBytes, std::string& errorMessage) const {
    outBytes.clear();

    if (currentImgBytes.empty() || worldSectors.empty()) {
        errorMessage = "Open a retail LVZ+IMG pair before extracting a real mesh resource.";
        return false;
    }

    for (const StorylandWorldSector& sector : worldSectors) {
        uint64_t cont = sector.imgOffset;
        uint64_t sectorEnd = std::min<uint64_t>(currentImgBytes.size(), sector.imgOffset + sector.byteSize);
        if (cont + 8 > sectorEnd) continue;

        uint32_t resourcesPointer = readU32(currentImgBytes, size_t(cont) + 0x00);
        uint32_t resourceCount = readU16(currentImgBytes, size_t(cont) + 0x04);
        if (resourceCount == 0 || resourceCount > 4096) continue;

        uint64_t listStart = cont + uint64_t(resourcesPointer) - 0x20ull;
        if (listStart < cont || listStart + uint64_t(resourceCount) * 8ull > sectorEnd) continue;

        struct ResourceRow {
            uint32_t id = 0;
            uint64_t rawOffset = 0;
        };

        std::vector<ResourceRow> rows;
        rows.reserve(resourceCount);

        for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
            uint64_t rowOffset64 = listStart + uint64_t(rowIndex) * 8ull;
            if (rowOffset64 + 8 > currentImgBytes.size()) continue;

            int32_t signedId = readI32(currentImgBytes, size_t(rowOffset64) + 0x00);
            if (signedId < 0) continue;

            uint32_t rawPointer = readU32(currentImgBytes, size_t(rowOffset64) + 0x04);
            uint64_t rawOffset = cont + uint64_t(rawPointer) - 0x20ull;
            if (rawOffset < cont || rawOffset >= sectorEnd) continue;

            rows.push_back({uint32_t(signedId), rawOffset});
        }

        if (rows.empty()) continue;

        std::sort(rows.begin(), rows.end(), [](const ResourceRow& lhs, const ResourceRow& rhs) {
            if (lhs.rawOffset != rhs.rawOffset) return lhs.rawOffset < rhs.rawOffset;
            return lhs.id < rhs.id;
        });

        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            if (rows[rowIndex].id != resourceId) continue;

            uint64_t rawEnd = sectorEnd;
            for (size_t nextIndex = rowIndex + 1; nextIndex < rows.size(); ++nextIndex) {
                if (rows[nextIndex].rawOffset > rows[rowIndex].rawOffset) {
                    rawEnd = rows[nextIndex].rawOffset;
                    break;
                }
            }

            if (rawEnd <= rows[rowIndex].rawOffset || rawEnd > currentImgBytes.size()) {
                errorMessage = "Located mesh resource span is invalid.";
                return false;
            }

            outBytes.assign(currentImgBytes.begin() + size_t(rows[rowIndex].rawOffset), currentImgBytes.begin() + size_t(rawEnd));
            return !outBytes.empty();
        }
    }

    errorMessage = "That real mesh resource id was not found in any sector resource table.";
    return false;
}

static bool candidateParsesAsSectorMeshPayload(
    const std::vector<uint8_t>& bytes,
    size_t candidateOffset,
    size_t maxEnd,
    uint32_t resourceId
) {
    StorylandWorldMesh mesh;
    return parseWorldOverlayMesh(bytes, candidateOffset, maxEnd, 0, resourceId, mesh) &&
           !mesh.vertices.empty() &&
           !mesh.triangles.empty();
}

static bool findRuntimeSafeSectorPayloadOffset(
    const std::vector<uint8_t>& bytes,
    uint64_t sectorImgOffset,
    uint64_t rawOffset,
    uint64_t rawEnd,
    uint32_t resourceId,
    size_t& outPayloadOffset
) {
    outPayloadOffset = size_t(rawOffset);
    if (rawOffset >= rawEnd || rawEnd > bytes.size()) return false;

    std::vector<size_t> candidates;

    // Prefer pointer/wrapper targets first.  The TLB address 0x78A7034C came
    // from the first material-row dword being read as a pointer, so replacing
    // from rawOffset is unsafe when the old resource starts with a pointer
    // wrapper.  Use the old wrapper's own local pointers to find the real
    // material/VIF payload that should be overwritten.
    for (size_t field = 0; field + 4 <= 0x80 && rawOffset + field + 4 <= rawEnd; field += 4) {
        uint32_t value = readU32(bytes, size_t(rawOffset + field));
        if (value < 4 || value == 0xAAAAAAAAu || value == 0xCCCCCCCCu || value == 0xFFFFFFFFu) continue;

        uint64_t asSectorLocal = sectorImgOffset + uint64_t(value) - 0x20ull;
        uint64_t asRawRelative = rawOffset + uint64_t(value);
        uint64_t asRawRelativeMinus20 = rawOffset + uint64_t(value) - 0x20ull;

        if (asSectorLocal >= rawOffset && asSectorLocal < rawEnd) candidates.push_back(size_t(asSectorLocal));
        if (asRawRelative >= rawOffset && asRawRelative < rawEnd) candidates.push_back(size_t(asRawRelative));
        if (asRawRelativeMinus20 >= rawOffset && asRawRelativeMinus20 < rawEnd) candidates.push_back(size_t(asRawRelativeMinus20));
    }

    // Then scan the old resource body.  This catches resources where the wrapper
    // pointer field is not one of the early dwords we know about.
    for (uint64_t cursor = rawOffset; cursor + 4 <= rawEnd && cursor < rawOffset + 0x2000ull; cursor += 4) {
        candidates.push_back(size_t(cursor));
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    for (size_t candidate : candidates) {
        if (candidate < rawOffset || candidate >= rawEnd) continue;
        if (candidateParsesAsSectorMeshPayload(bytes, candidate, size_t(rawEnd), resourceId)) {
            outPayloadOffset = candidate;
            return true;
        }
    }

    return false;
}

bool StorylandArchiveBrowser::replaceWorldMeshResourceBytes(uint32_t resourceId, const std::vector<uint8_t>& replacementBytes, std::string& report, std::string& errorMessage) {
    report.clear();

    if (replacementBytes.empty()) {
        errorMessage = "Replacement file is empty.";
        return false;
    }
    if (currentImgBytes.empty() || currentLvzBytes.empty() || worldSectors.empty()) {
        errorMessage = "Open a retail LVZ+IMG pair before replacing a real mesh resource.";
        return false;
    }

    bool replacementWasConvertedFromMdl = false;
    std::vector<uint8_t> replacementPayload = normalizeWorldMeshReplacementPayload(replacementBytes);
    if (replacementPayload.empty()) {
        errorMessage = "Replacement resolved to an empty payload.";
        return false;
    }
    if (replacementPayload.size() > 0x08000000u) {
        errorMessage = "Replacement mesh resource is too large.";
        return false;
    }

    StorylandWorldMesh validationMesh;
    if (!parseWorldOverlayMesh(replacementPayload, 0, replacementPayload.size(), 0, resourceId, validationMesh) ||
        validationMesh.vertices.empty() ||
        validationMesh.triangles.empty()) {
        uint32_t fallbackTextureId = 0;
        for (const StorylandWorldMesh& mesh : worldMeshCache) {
            if (mesh.resourceIndex != resourceId) continue;
            for (const StorylandWorldMeshTriangle& triangle : mesh.triangles) {
                if (triangle.textureId != 0xFFFFFFFFu) {
                    fallbackTextureId = triangle.textureId;
                    break;
                }
            }
            break;
        }

        std::vector<uint8_t> convertedPayload;
        if (!buildWorldSectorMeshPayloadFromLeedsChunk(replacementBytes, fallbackTextureId, convertedPayload)) {
            errorMessage =
                "Storyland could not convert the selected file into a WRLD sector mesh resource payload. "
                "The file must contain parseable Leeds/MDL strip geometry or already be a raw WRLD sector mesh resource payload.";
            return false;
        }

        replacementPayload = std::move(convertedPayload);
        replacementWasConvertedFromMdl = true;

        StorylandWorldMesh convertedValidationMesh;
        if (!parseWorldOverlayMesh(replacementPayload, 0, replacementPayload.size(), 0, resourceId, convertedValidationMesh) ||
            convertedValidationMesh.vertices.empty() ||
            convertedValidationMesh.triangles.empty()) {
            errorMessage =
                "Storyland generated a WRLD sector mesh payload from the selected file, but the generated payload failed validation. "
                "Replacement was not applied.";
            return false;
        }
    }

    std::vector<StorylandSectorResourceSpan> spans;

    for (const StorylandWorldSector& sector : worldSectors) {
        uint64_t cont = sector.imgOffset;
        uint64_t sectorEnd = std::min<uint64_t>(currentImgBytes.size(), sector.imgOffset + sector.byteSize);
        if (cont + 8 > sectorEnd) continue;

        uint32_t resourcesPointer = readU32(currentImgBytes, size_t(cont) + 0x00);
        uint32_t resourceCount = readU16(currentImgBytes, size_t(cont) + 0x04);
        if (resourceCount == 0 || resourceCount > 4096) continue;

        uint64_t listStart = cont + uint64_t(resourcesPointer) - 0x20ull;
        if (listStart < cont || listStart + uint64_t(resourceCount) * 8ull > sectorEnd) continue;

        struct ResourceRow {
            uint32_t id = 0;
            uint64_t rowOffset = 0;
            uint64_t rawOffset = 0;
        };

        std::vector<ResourceRow> rows;
        rows.reserve(resourceCount);

        for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
            uint64_t rowOffset64 = listStart + uint64_t(rowIndex) * 8ull;
            if (rowOffset64 + 8 > currentImgBytes.size()) continue;

            int32_t signedId = readI32(currentImgBytes, size_t(rowOffset64) + 0x00);
            if (signedId < 0) continue;

            uint32_t rawPointer = readU32(currentImgBytes, size_t(rowOffset64) + 0x04);
            uint64_t rawOffset = cont + uint64_t(rawPointer) - 0x20ull;
            if (rawOffset < cont || rawOffset >= sectorEnd) continue;

            rows.push_back({uint32_t(signedId), rowOffset64, rawOffset});
        }

        if (rows.empty()) continue;

        std::sort(rows.begin(), rows.end(), [](const ResourceRow& lhs, const ResourceRow& rhs) {
            if (lhs.rawOffset != rhs.rawOffset) return lhs.rawOffset < rhs.rawOffset;
            return lhs.id < rhs.id;
        });

        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            if (rows[rowIndex].id != resourceId) continue;

            uint64_t rawEnd = sectorEnd;
            for (size_t nextIndex = rowIndex + 1; nextIndex < rows.size(); ++nextIndex) {
                if (rows[nextIndex].rawOffset > rows[rowIndex].rawOffset) {
                    rawEnd = rows[nextIndex].rawOffset;
                    break;
                }
            }

            if (rawEnd <= rows[rowIndex].rawOffset) continue;

            StorylandSectorResourceSpan span;
            span.sectorIndex = sector.sectorIndex;
            span.resourceId = resourceId;
            span.sectorImgOffset = sector.imgOffset;
            span.sectorByteSize = sector.byteSize;
            span.sectorHeaderOffset = sector.headerOffset;
            span.resourceRowOffset = rows[rowIndex].rowOffset;
            span.rawOffset = rows[rowIndex].rawOffset;
            span.rawEnd = rawEnd;
            spans.push_back(span);
            break;
        }
    }

    if (spans.empty()) {
        errorMessage = "That real mesh resource id was not found in any sector resource table.";
        return false;
    }

    uint64_t totalCapacity = 0;
    uint64_t totalWritten = 0;
    uint32_t replacedCount = 0;
    uint32_t wrapperOffsetsPreserved = 0;

    for (const StorylandSectorResourceSpan& span : spans) {
        if (span.sectorImgOffset > currentImgBytes.size() ||
            span.sectorImgOffset + span.sectorByteSize > currentImgBytes.size() ||
            span.rawOffset > currentImgBytes.size() ||
            span.rawEnd > currentImgBytes.size() ||
            span.rawEnd <= span.rawOffset) {
            errorMessage = "A target mesh resource span is invalid.";
            return false;
        }

        size_t payloadOffset = 0;
        if (!findRuntimeSafeSectorPayloadOffset(currentImgBytes, span.sectorImgOffset, span.rawOffset, span.rawEnd, resourceId, payloadOffset)) {
            errorMessage =
                "Could not locate the existing runtime-safe sector mesh payload inside the selected resource. "
                "Replacement was not applied because redirecting Resource[] directly to new bytes can cause the game to read material rows as pointers and TLB-miss.";
            return false;
        }

        size_t capacity = size_t(span.rawEnd) - payloadOffset;
        if (replacementPayload.size() > capacity) {
            errorMessage =
                "Converted replacement is larger than the existing runtime-safe payload budget.\r\n"
                "Selected resource id: " + std::to_string(resourceId) + "\r\n"
                "Payload capacity: " + std::to_string(capacity) + " bytes\r\n"
                "Replacement payload: " + std::to_string(replacementPayload.size()) + " bytes\r\n\r\n"
                "Storyland refused to grow/redirect this pointer-backed sector resource because that is what caused the 0x78A7034C-style TLB misses.";
            return false;
        }

        std::copy(replacementPayload.begin(), replacementPayload.end(), currentImgBytes.begin() + payloadOffset);

        // Clear the remaining old bytes so stale VIF/material data cannot be
        // walked after the new stream ends.  Do not change Resource[] pointers,
        // WRLD sizes, or later IMG offsets.
        std::fill(
            currentImgBytes.begin() + payloadOffset + replacementPayload.size(),
            currentImgBytes.begin() + size_t(span.rawEnd),
            0
        );

        if (payloadOffset != size_t(span.rawOffset)) wrapperOffsetsPreserved++;
        totalCapacity += capacity;
        totalWritten += replacementPayload.size();
        replacedCount++;
    }

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        errorMessage = "Mesh resource replacement was applied in memory, but the LVZ/IMG browser could not rebuild its parsed index: " + rebuildError;
        return false;
    }

    uint32_t parsedAfter = 0;
    uint32_t trianglesAfter = 0;
    for (const StorylandWorldMesh& mesh : worldMeshCache) {
        if (mesh.resourceIndex != resourceId) continue;
        parsedAfter++;
        trianglesAfter += uint32_t(mesh.triangles.size());
    }

    report =
        "Runtime-safe real sector mesh replacement\r\n"
        "Resource id preserved: " + std::to_string(resourceId) + "\r\n"
        "Input file converted to sector-resource payload: " + std::string(replacementWasConvertedFromMdl ? "yes; exact MDL VIF packets copied when possible; 24-byte sector material rows" : "no; already sector payload") + "\r\n"
        "Sector Resource[] rows redirected: 0; existing Resource[] pointer was preserved\r\n"
        "Wrapper/pointer starts preserved: " + std::to_string(wrapperOffsetsPreserved) + "\r\n"
        "Occurrences replaced in-place: " + std::to_string(replacedCount) + "\r\n"
        "Total runtime-safe payload capacity: " + std::to_string(totalCapacity) + " bytes\r\n"
        "Total replacement payload bytes written: " + std::to_string(totalWritten) + " bytes\r\n"
        "IMG growth: 0 bytes\r\n"
        "Later LVZ IMG offsets shifted: no\r\n"
        "WRLD sector chunk sizes changed: no\r\n"
        "Parsed replacement mesh variants after rebuild: " + std::to_string(parsedAfter) + "\r\n"
        "Parsed replacement triangles after rebuild: " + std::to_string(trianglesAfter) + "\r\n"
        "This path avoids the TLB bug where the game read the first material row dword as a pointer.\r\n"
        "Use File > Export/Rebuild LVZ+IMG Pair to write the edited files.";

    return true;
}




bool StorylandArchiveBrowser::changeWorldMeshResourceId(uint32_t oldResourceId, uint32_t newResourceId, std::string& report, std::string& errorMessage) {
    report.clear();

    if (oldResourceId == newResourceId) {
        errorMessage = "Old and new resource ids are the same.";
        return false;
    }
    if (newResourceId > 0xFFFFu) {
        errorMessage = "New resource id must fit the 16-bit sGeomInstance resource id field.";
        return false;
    }
    if (currentImgBytes.empty() || currentLvzBytes.empty() || worldSectors.empty()) {
        errorMessage = "Open a retail LVZ+IMG pair before changing a mesh resource id.";
        return false;
    }

    uint32_t tableRowsChanged = 0;
    uint32_t placementRowsChanged = 0;

    for (const StorylandWorldSector& sector : worldSectors) {
        uint64_t cont = sector.imgOffset;
        uint64_t sectorEnd = std::min<uint64_t>(currentImgBytes.size(), sector.imgOffset + sector.byteSize);
        if (cont + 8 > sectorEnd) continue;

        uint32_t resourcesPointer = readU32(currentImgBytes, size_t(cont) + 0x00);
        uint32_t resourceCount = readU16(currentImgBytes, size_t(cont) + 0x04);
        if (resourceCount == 0 || resourceCount > 4096) continue;

        uint64_t listStart = cont + uint64_t(resourcesPointer) - 0x20ull;
        if (listStart < cont || listStart + uint64_t(resourceCount) * 8ull > sectorEnd) continue;

        for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
            uint64_t rowOffset64 = listStart + uint64_t(rowIndex) * 8ull;
            if (rowOffset64 + 8 > currentImgBytes.size()) continue;

            int32_t signedId = readI32(currentImgBytes, size_t(rowOffset64) + 0x00);
            if (signedId < 0) continue;
            if (uint32_t(signedId) != oldResourceId) continue;

            writeU32(currentImgBytes, size_t(rowOffset64) + 0x00, newResourceId);
            tableRowsChanged++;
        }
    }

    for (const StorylandWorldPlacement& placement : worldPlacements) {
        if (placement.resourceIndex != oldResourceId) continue;
        if (placement.imgOffset + 0x04 > currentImgBytes.size()) continue;

        writeU16(currentImgBytes, size_t(placement.imgOffset) + 0x02, uint16_t(newResourceId));
        placementRowsChanged++;
    }

    if (tableRowsChanged == 0 && placementRowsChanged == 0) {
        errorMessage = "No sector Resource[] rows or placement rows used the selected resource id.";
        return false;
    }

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        errorMessage = "Resource id was changed in memory, but the LVZ/IMG browser could not rebuild its parsed index: " + rebuildError;
        return false;
    }

    report =
        "Changed real sector mesh resource id\r\n"
        "Old id: " + std::to_string(oldResourceId) + "\r\n"
        "New id: " + std::to_string(newResourceId) + "\r\n"
        "Resource[] table rows changed: " + std::to_string(tableRowsChanged) + "\r\n"
        "sGeomInstance placement rows changed: " + std::to_string(placementRowsChanged) + "\r\n"
        "Use File > Export/Rebuild LVZ+IMG Pair to write the edited files.";

    return true;
}





bool StorylandArchiveBrowser::saveLvzImgPair(const std::wstring& lvzPath, const std::wstring& imgPath, bool compressLvz, std::string& errorMessage) const {
    if (currentLvzBytes.empty() || currentImgBytes.empty()) {
        errorMessage = "No LVZ+IMG pair is loaded.";
        return false;
    }

    std::vector<uint8_t> lvzOut;
    if (compressLvz) {
        if (!deflateLvzBytes(currentLvzBytes, lvzOut, errorMessage)) return false;
    } else {
        lvzOut = currentLvzBytes;
    }

    if (!writeWholeFile(lvzPath, lvzOut, errorMessage)) {
        errorMessage = "LVZ output failed.\r\n" + errorMessage;
        return false;
    }
    if (!writeWholeFile(imgPath, currentImgBytes, errorMessage)) {
        errorMessage = "IMG output failed.\r\n" + errorMessage;
        return false;
    }
    return true;
}

bool StorylandArchiveBrowser::overwriteCurrentLvzImgPair(bool compressLvz, std::string& errorMessage) const {
    if (currentLvzPath.empty() || currentImgPath.empty()) {
        errorMessage = "No current LVZ+IMG paths are loaded.";
        return false;
    }
    return saveLvzImgPair(currentLvzPath, currentImgPath, compressLvz, errorMessage);
}


bool StorylandArchiveBrowser::findEntryByStemAndExtension(const std::wstring& stem, const std::initializer_list<std::wstring>& extensions, size_t& outIndex) const {
    std::wstring stemLower = lowerWide(stem);

    for (size_t i = 0; i < archiveEntries.size(); ++i) {
        std::wstring wideName(archiveEntries[i].name.begin(), archiveEntries[i].name.end());
        std::wstring nameStem = lowerWide(getFileStemPart(wideName));
        if (nameStem != stemLower) continue;

        std::wstring ext = getExtensionLower(wideName);
        for (const auto& wanted : extensions) {
            if (ext == lowerWide(wanted)) {
                outIndex = i;
                return true;
            }
        }
    }

    return false;
}
