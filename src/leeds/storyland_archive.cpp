#include "storyland_archive.h"
#include "storyland_atomic_io.h"
#include "storyland_dma_validator.h"
#include "storyland_model.h"
#include "leeds_texture.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <limits>
#include <iterator>
#include <set>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
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
static constexpr uint64_t STORYLAND_MAX_ARCHIVE_FILE_BYTES = 2ull * 1024ull * 1024ull * 1024ull;
static constexpr size_t STORYLAND_MAX_LVZ_INFLATED_BYTES = 1024ull * 1024ull * 1024ull;

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

static uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 2 > bytes.size()) return 0;
    return uint16_t(bytes[offset + 0]) |
           uint16_t(uint16_t(bytes[offset + 1]) << 8);
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

static void writeF32(std::vector<uint8_t>& bytes, size_t offset, float value) {
    if (offset + 4 > bytes.size()) return;
    uint32_t raw = 0u;
    std::memcpy(&raw, &value, sizeof(raw));
    writeU32(bytes, offset, raw);
}

static uint16_t floatToHalf(float value) {
    uint32_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint32_t sign = (bits >> 16u) & 0x8000u;
    int32_t exponent = int32_t((bits >> 23u) & 0xFFu) - 127 + 15;
    uint32_t mantissa = bits & 0x007FFFFFu;

    if (((bits >> 23u) & 0xFFu) == 0xFFu) {
        if (mantissa != 0u) return uint16_t(sign | 0x7E00u);
        return uint16_t(sign | 0x7C00u);
    }
    if (exponent <= 0) {
        if (exponent < -10) return uint16_t(sign);
        mantissa |= 0x00800000u;
        const uint32_t shift = uint32_t(14 - exponent);
        uint32_t halfMantissa = mantissa >> shift;
        if ((mantissa >> (shift - 1u)) & 1u) halfMantissa++;
        return uint16_t(sign | (halfMantissa & 0x03FFu));
    }
    if (exponent >= 31) return uint16_t(sign | 0x7BFFu);
    uint32_t half = sign | (uint32_t(exponent) << 10u) | (mantissa >> 13u);
    if (mantissa & 0x00001000u) half++;
    return uint16_t(half & 0xFFFFu);
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


struct StorylandMasterResourceTableInfo {
    uint32_t tableOffset = 0;
    uint32_t count = 0;
    uint32_t stride = 12;
    uint32_t dataEnd = 0;
};

static bool locateMasterResourceTable(const std::vector<uint8_t>& bytes, StorylandMasterResourceTableInfo& outInfo) {
    outInfo = {};
    if (bytes.size() < 0x150u || readU32(bytes, 0x00u) != WRLD_IDENT) return false;

    uint32_t dataEnd = readU32(bytes, 0x0Cu);
    if (dataEnd < 0x150u || dataEnd > bytes.size()) dataEnd = uint32_t(bytes.size());

    const uint32_t table = readU32(bytes, 0x20u);
    if (table < 0x40u || table >= dataEnd || (table & 3u) != 0u) return false;

    auto tableLooksValid = [&](uint32_t count) -> bool {
        if (count == 0u || count > 65536u) return false;
        if (uint64_t(table) + uint64_t(count) * 12ull > uint64_t(dataEnd)) return false;

        const uint32_t sampleCount = std::min<uint32_t>(count, 256u);
        uint32_t goodRows = 0u;
        for (uint32_t i = 0; i < sampleCount; ++i) {
            const size_t row = size_t(table) + size_t(i) * 12u;
            const uint32_t pointer = readU32(bytes, row + 0u);
            const uint32_t resourceId = readU32(bytes, row + 8u);
            const bool idOk = resourceId == i || resourceId == 0xFFFFFFFFu;
            const bool pointerOk = pointer == 0u || pointer == 0xFFFFFFFFu ||
                (((pointer & 3u) == 0u) && pointer >= 0x40u && pointer < dataEnd);
            if (idOk && pointerOk) ++goodRows;
        }
        return sampleCount != 0u && goodRows * 5u >= sampleCount * 4u;
    };

    // Retail VCS/LCS master WRLD stores Resource[] count at +0x14C.  This is
    // separate from +0x14, which is the relocation-entry count.
    uint32_t count = readU32(bytes, 0x14Cu);
    if (!tableLooksValid(count)) {
        // Conservative fallback for unusual builds: infer the contiguous 12-byte
        // Resource[] rows from their stable {pointer, unknown, id} layout.
        count = 0u;
        for (uint32_t i = 0; i < 65536u; ++i) {
            const uint64_t row64 = uint64_t(table) + uint64_t(i) * 12ull;
            if (row64 + 12ull > dataEnd) break;
            const size_t row = size_t(row64);
            const uint32_t pointer = readU32(bytes, row + 0u);
            const uint32_t resourceId = readU32(bytes, row + 8u);
            const bool idOk = resourceId == i || resourceId == 0xFFFFFFFFu;
            const bool pointerOk = pointer == 0u || pointer == 0xFFFFFFFFu ||
                (((pointer & 3u) == 0u) && pointer >= 0x40u && pointer < dataEnd);
            if (!idOk || !pointerOk) break;
            count = i + 1u;
        }
        if (!tableLooksValid(count)) return false;
    }

    outInfo.tableOffset = table;
    outInfo.count = count;
    outInfo.stride = 12u;
    outInfo.dataEnd = dataEnd;
    return true;
}

static bool masterResourceRow(const std::vector<uint8_t>& bytes, uint32_t resourceId,
                              StorylandMasterResourceTableInfo& tableInfo,
                              size_t& rowOffset, uint32_t& pointer) {
    if (!locateMasterResourceTable(bytes, tableInfo) || resourceId >= tableInfo.count) return false;
    rowOffset = size_t(tableInfo.tableOffset) + size_t(resourceId) * size_t(tableInfo.stride);
    if (rowOffset + 12u > tableInfo.dataEnd) return false;
    pointer = readU32(bytes, rowOffset + 0u);
    return true;
}

static size_t masterResourcePayloadEnd(const std::vector<uint8_t>& bytes,
                                       const StorylandMasterResourceTableInfo& tableInfo,
                                       uint32_t pointer) {
    size_t end = tableInfo.dataEnd;
    for (uint32_t i = 0; i < tableInfo.count; ++i) {
        const size_t row = size_t(tableInfo.tableOffset) + size_t(i) * size_t(tableInfo.stride);
        if (row + 12u > tableInfo.dataEnd) break;
        const uint32_t candidate = readU32(bytes, row + 0u);
        if (candidate > pointer && candidate < end && candidate < tableInfo.dataEnd) end = candidate;
    }
    return end;
}

static bool appendPayloadToMasterResourceSlot(
    std::vector<uint8_t>& lvzBytes,
    uint32_t resourceId,
    const std::vector<uint8_t>& payload,
    uint32_t& outPayloadOffset,
    std::string& errorMessage
) {
    outPayloadOffset = 0u;
    if (payload.empty()) {
        errorMessage = "Converted master Resource[] payload is empty.";
        return false;
    }

    StorylandMasterResourceTableInfo tableInfo;
    size_t rowOffset = 0u;
    uint32_t oldPointer = 0u;
    if (!masterResourceRow(lvzBytes, resourceId, tableInfo, rowOffset, oldPointer)) {
        errorMessage = "The master WRLD Resource[] table is missing or the requested resource id is outside it.";
        return false;
    }

    const uint32_t relocationOffset = readU32(lvzBytes, 0x0Cu);
    const uint32_t relocationOffset2 = readU32(lvzBytes, 0x10u);
    const uint32_t relocationCount = readU32(lvzBytes, 0x14u);
    if (relocationOffset != relocationOffset2 || relocationOffset != tableInfo.dataEnd ||
        uint64_t(relocationOffset) + uint64_t(relocationCount) * 4ull != lvzBytes.size()) {
        errorMessage = "The LVZ relocation table is not in the retail end-of-file layout required for safe Resource[] insertion.";
        return false;
    }

    std::vector<uint32_t> relocations;
    relocations.reserve(size_t(relocationCount) + 1u);
    for (uint32_t i = 0; i < relocationCount; ++i) {
        relocations.push_back(readU32(lvzBytes, size_t(relocationOffset) + size_t(i) * 4u));
    }

    std::vector<uint8_t> data(lvzBytes.begin(), lvzBytes.begin() + relocationOffset);
    while ((data.size() & 0x0Fu) != 0u) data.push_back(0u);
    if (data.size() > 0xFFFFFFFFull || payload.size() > 0xFFFFFFFFull - data.size()) {
        errorMessage = "The converted Resource[] payload would exceed the 32-bit LVZ address space.";
        return false;
    }

    const uint32_t payloadOffset = uint32_t(data.size());
    data.insert(data.end(), payload.begin(), payload.end());
    while ((data.size() & 3u) != 0u) data.push_back(0u);

    // Resource[] rows are {pointer, unknown/flags, resource id}.  Empty retail
    // rows are {0, 0, 0xFFFFFFFF}; activation only changes the pointer/id.
    writeU32(data, rowOffset + 0u, payloadOffset);
    writeU32(data, rowOffset + 4u, 0u);
    writeU32(data, rowOffset + 8u, resourceId);

    const uint32_t pointerFieldOffset = uint32_t(rowOffset);
    if (std::find(relocations.begin(), relocations.end(), pointerFieldOffset) == relocations.end()) {
        relocations.push_back(pointerFieldOffset);
    }
    std::sort(relocations.begin(), relocations.end());
    relocations.erase(std::unique(relocations.begin(), relocations.end()), relocations.end());

    const uint32_t newRelocationOffset = uint32_t(data.size());
    for (uint32_t relocation : relocations) {
        data.push_back(uint8_t(relocation & 0xFFu));
        data.push_back(uint8_t((relocation >> 8) & 0xFFu));
        data.push_back(uint8_t((relocation >> 16) & 0xFFu));
        data.push_back(uint8_t((relocation >> 24) & 0xFFu));
    }

    if (data.size() > 0xFFFFFFFFull) {
        errorMessage = "The rebuilt LVZ exceeds the 32-bit file-size field.";
        return false;
    }

    writeU32(data, 0x08u, uint32_t(data.size()));
    writeU32(data, 0x0Cu, newRelocationOffset);
    writeU32(data, 0x10u, newRelocationOffset);
    writeU32(data, 0x14u, uint32_t(relocations.size()));

    lvzBytes.swap(data);
    outPayloadOffset = payloadOffset;
    return true;
}

static bool mobileLcsRwChunkHeaderValid(const std::vector<uint8_t>& bytes, size_t offset, size_t limit,
                                        uint32_t expectedType = 0xFFFFFFFFu) {
    if (offset + 12u > limit || limit > bytes.size()) return false;
    uint32_t type = readU32(bytes, offset + 0u);
    uint32_t size = readU32(bytes, offset + 4u);
    uint32_t version = readU32(bytes, offset + 8u);
    if (expectedType != 0xFFFFFFFFu && type != expectedType) return false;
    if (version != 0x00000310u && version != 0x1003FFFFu) return false;
    if (size > limit - offset - 12u) return false;
    return true;
}

static bool mobileLcsRawClumpAt(const std::vector<uint8_t>& bytes, size_t offset, size_t& outSize) {
    outSize = 0;
    if (!mobileLcsRwChunkHeaderValid(bytes, offset, bytes.size(), 0x10u)) return false;
    uint32_t payloadSize = readU32(bytes, offset + 4u);
    size_t clumpEnd = offset + 12u + size_t(payloadSize);
    if (payloadSize < 32u || clumpEnd > bytes.size()) return false;

    size_t child = offset + 12u;
    if (!mobileLcsRwChunkHeaderValid(bytes, child, clumpEnd, 0x01u)) return false;
    uint32_t structSize = readU32(bytes, child + 4u);
    if (structSize != 4u && structSize != 12u) return false;
    child += 12u + size_t(structSize);
    if (!mobileLcsRwChunkHeaderValid(bytes, child, clumpEnd, 0x0Eu)) return false;

    outSize = 12u + size_t(payloadSize);
    return true;
}

static std::string mobileLcsSanitizeEntryStem(std::string value) {
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '.')) value.pop_back();
    for (char& ch : value) {
        unsigned char byte = static_cast<unsigned char>(ch);
        if (!(std::isalnum(byte) || ch == '_' || ch == '-')) ch = '_';
    }
    while (!value.empty() && value.front() == '_') value.erase(value.begin());
    if (value.size() > 80u) value.resize(80u);
    return value;
}

static std::string mobileLcsFrameNameFromClump(const std::vector<uint8_t>& bytes, size_t offset, size_t size) {
    const uint32_t frameNamePlugin = 0x0253F2FEu;
    size_t end = std::min(bytes.size(), offset + size);
    std::string first;
    std::string preferred;
    for (size_t probe = offset + 12u; probe + 12u <= end; ++probe) {
        if (readU32(bytes, probe) != frameNamePlugin) continue;
        uint32_t payloadSize = readU32(bytes, probe + 4u);
        uint32_t version = readU32(bytes, probe + 8u);
        if (version != 0x00000310u || payloadSize == 0u || payloadSize > 128u || probe + 12u + payloadSize > end) continue;
        std::string name;
        for (size_t i = 0; i < payloadSize; ++i) {
            unsigned char ch = bytes[probe + 12u + i];
            if (ch == 0) break;
            if (ch < 32 || ch >= 127) { name.clear(); break; }
            name.push_back(char(ch));
        }
        name = mobileLcsSanitizeEntryStem(name);
        if (name.empty()) continue;
        if (first.empty()) first = name;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (lower != "root" && lower != "scene_root" && lower.find("dummy") == std::string::npos) {
            preferred = name;
            break;
        }
    }
    return preferred.empty() ? first : preferred;
}

static bool mobileLcsRwStringChunk(
    const std::vector<uint8_t>& bytes,
    size_t offset,
    size_t limit,
    std::string& textOut,
    size_t& nextOffsetOut
) {
    textOut.clear();
    nextOffsetOut = offset;
    if (!mobileLcsRwChunkHeaderValid(bytes, offset, limit, 0x02u)) return false;

    uint32_t payloadSize = readU32(bytes, offset + 4u);
    if (payloadSize > 1024u) return false;
    size_t payloadOffset = offset + 12u;
    size_t payloadEnd = payloadOffset + size_t(payloadSize);
    for (size_t cursor = payloadOffset; cursor < payloadEnd; ++cursor) {
        unsigned char ch = bytes[cursor];
        if (ch == 0u) break;
        if (ch < 32u || ch >= 127u) {
            textOut.clear();
            return false;
        }
        textOut.push_back(char(ch));
    }

    nextOffsetOut = payloadEnd;
    return true;
}

static bool mobileLcsRawTextureDictionaryAt(
    const std::vector<uint8_t>& bytes,
    size_t offset,
    size_t& outSize,
    std::vector<std::string>& textureNamesOut
) {
    outSize = 0u;
    textureNamesOut.clear();
    if (offset + 12u > bytes.size()) return false;
    if (readU32(bytes, offset + 0u) != 0x16u || readU32(bytes, offset + 8u) != 0x00000310u) return false;

    size_t limit = std::min(bytes.size(), offset + size_t(0x04000000u));
    size_t cursor = offset + 12u;
    if (!mobileLcsRwChunkHeaderValid(bytes, cursor, limit, 0x01u)) return false;
    uint32_t dictionaryStructSize = readU32(bytes, cursor + 4u);
    if (dictionaryStructSize < 4u || dictionaryStructSize > 32u) return false;
    uint32_t textureCount = readU16(bytes, cursor + 12u);
    if (textureCount == 0u || textureCount > 4096u) return false;
    cursor += 12u + size_t(dictionaryStructSize);

    textureNamesOut.reserve(textureCount);
    for (uint32_t textureIndex = 0u; textureIndex < textureCount; ++textureIndex) {
        if (cursor + 12u > limit) return false;
        if (readU32(bytes, cursor + 0u) != 0x15u || readU32(bytes, cursor + 8u) != 0x00000310u) return false;

        size_t child = cursor + 12u;
        if (!mobileLcsRwChunkHeaderValid(bytes, child, limit, 0x01u)) return false;
        uint32_t platformStructSize = readU32(bytes, child + 4u);
        if (platformStructSize < 8u || platformStructSize > 64u) return false;
        if (child + 12u + 4u > limit) return false;
        if (readU32(bytes, child + 12u) != 0x00505350u) return false; // "PSP\0"
        child += 12u + size_t(platformStructSize);

        std::string textureName;
        size_t nextChild = child;
        if (!mobileLcsRwStringChunk(bytes, child, limit, textureName, nextChild)) return false;
        child = nextChild;

        std::string maskName;
        if (!mobileLcsRwStringChunk(bytes, child, limit, maskName, nextChild)) return false;
        child = nextChild;

        if (!mobileLcsRwChunkHeaderValid(bytes, child, limit, 0x01u)) return false;
        size_t rasterContainerEnd = child + 12u + size_t(readU32(bytes, child + 4u));
        size_t rasterChild = child + 12u;
        if (!mobileLcsRwChunkHeaderValid(bytes, rasterChild, rasterContainerEnd, 0x01u)) return false;
        if (readU32(bytes, rasterChild + 4u) < 20u) return false;
        rasterChild += 12u + size_t(readU32(bytes, rasterChild + 4u));
        if (!mobileLcsRwChunkHeaderValid(bytes, rasterChild, rasterContainerEnd, 0x01u)) return false;

        textureName = mobileLcsSanitizeEntryStem(textureName);
        if (textureName.empty()) {
            char fallback[48] = {};
            std::snprintf(fallback, sizeof(fallback), "texture_%04u", textureIndex);
            textureName = fallback;
        }
        textureNamesOut.push_back(textureName);

        cursor = rasterContainerEnd;
        if (mobileLcsRwChunkHeaderValid(bytes, cursor, limit, 0x03u)) {
            cursor += 12u + size_t(readU32(bytes, cursor + 4u));
        }
    }

    if (mobileLcsRwChunkHeaderValid(bytes, cursor, limit, 0x03u)) {
        cursor += 12u + size_t(readU32(bytes, cursor + 4u));
    }

    if (cursor <= offset || cursor > limit) return false;
    outSize = cursor - offset;
    return true;
}

static std::vector<std::string> mobileLcsTextureNameHintsFromClump(
    const std::vector<uint8_t>& bytes,
    size_t offset,
    size_t size
) {
    std::vector<std::string> hints;
    std::set<std::string> seen;
    size_t end = std::min(bytes.size(), offset + size);

    for (size_t probe = offset + 12u; probe + 12u <= end; ++probe) {
        if (readU32(bytes, probe + 0u) != 0x02u || readU32(bytes, probe + 8u) != 0x00000310u) continue;
        uint32_t payloadSize = readU32(bytes, probe + 4u);
        if (payloadSize == 0u || payloadSize > 128u || probe + 12u + payloadSize > end) continue;

        std::string value;
        for (uint32_t i = 0u; i < payloadSize; ++i) {
            unsigned char ch = bytes[probe + 12u + i];
            if (ch == 0u) break;
            if (ch < 32u || ch >= 127u) {
                value.clear();
                break;
            }
            value.push_back(char(ch));
        }
        if (value.empty()) continue;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (seen.insert(value).second) hints.push_back(value);
    }
    return hints;
}

static std::string mobileLcsTextureMatchKey(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
    return value;
}

static std::string mobileLcsTextureLooseKey(std::string value) {
    value = mobileLcsTextureMatchKey(std::move(value));
    const char* prefixes[] = {"xv_", "veh_", "vehicle_", "lcs_"};
    for (const char* prefix : prefixes) {
        size_t length = std::strlen(prefix);
        if (value.size() > length && value.compare(0u, length, prefix) == 0) {
            value.erase(0u, length);
            break;
        }
    }
    return value;
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

static bool parseWorldOverlayMeshNear(
    const std::vector<uint8_t>& bytes,
    size_t rawOffset,
    size_t maxEnd,
    uint32_t sectorIndex,
    uint32_t resourceIndex,
    StorylandWorldMesh& outMesh,
    size_t& descriptorOffset
) {
    descriptorOffset = rawOffset;
    if (parseWorldOverlayMesh(bytes, rawOffset, maxEnd, sectorIndex, resourceIndex, outMesh)) return true;
    size_t searchEnd = std::min(maxEnd, rawOffset + 0x180u);
    for (size_t offset = alignUp4Size(rawOffset + 4u); offset + 4u <= searchEnd; offset += 4u) {
        if (!parseWorldOverlayMesh(bytes, offset, maxEnd, sectorIndex, resourceIndex, outMesh)) continue;
        descriptorOffset = offset;
        return true;
    }
    return false;
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

static bool decodeLegacyTextureReference(
    const std::vector<uint8_t>& bytes,
    size_t start,
    size_t end,
    StorylandDirectTextureResource& outTexture
) {
    if (end > bytes.size()) end = bytes.size();
    if (start + 64 >= end || end - start > 32u * 1024u * 1024u) return false;
    size_t indexBytes = end - start - 64u;
    size_t pixels = indexBytes * 2u;
    int chosenW = 0, chosenH = 0;
    const int dims[] = {16, 32, 64, 128, 256, 512, 1024};
    for (int width : dims) {
        if (pixels % size_t(width) != 0) continue;
        size_t height = pixels / size_t(width);
        if (height > 1024) continue;
        bool supportedHeight = std::find(std::begin(dims), std::end(dims), int(height)) != std::end(dims);
        if (!supportedHeight) continue;
        if (chosenW == 0 || std::abs(width - int(height)) < std::abs(chosenW - chosenH)) {
            chosenW = width;
            chosenH = int(height);
        }
    }
    if (chosenW <= 0 || chosenH <= 0) return false;
    if (!directRasterLooksUseful(bytes, start, indexBytes) || !directPaletteLooksUseful(bytes, end - 64u, 64u)) return false;

    std::vector<uint8_t> indices = directTextureExpandNibblesLoFirst(bytes.data() + start, indexBytes, pixels);
    indices = directTextureUnswizzlePs2Indices(indices, chosenW, chosenH);
    outTexture.headerOffset = uint32_t(start);
    outTexture.dataOffset = uint32_t(start);
    outTexture.width = chosenW;
    outTexture.height = chosenH;
    outTexture.bpp = 4;
    outTexture.legacyRaw4bpp = true;
    outTexture.rgba.assign(size_t(chosenW) * size_t(chosenH) * 4u, 255);
    size_t palette = end - 64u;
    for (int y = 0; y < chosenH; ++y) {
        for (int x = 0; x < chosenW; ++x) {
            size_t pixel = size_t(y) * size_t(chosenW) + size_t(x);
            size_t pal = palette + size_t(indices[pixel] & 0x0Fu) * 4u;
            writeDirectTexturePixelFlipped(outTexture, x, y,
                bytes[pal], bytes[pal + 1], bytes[pal + 2],
                uint8_t(std::min(255, int(bytes[pal + 3]) * 255 / 128)));
        }
    }
    return true;
}

}

void StorylandArchiveBrowser::clear() {
    currentImgPath.clear();
    currentLvzPath.clear();
    currentImgSize = 0;
    currentLevelSummary.clear();
    currentImgKind = StorylandImgKind::None;
    currentLvzBytes.clear();
    currentImgBytes.clear();
    archiveEntries.clear();
    worldPlacements.clear();
    worldSectors.clear();
    worldMeshCache.clear();
    masterMeshResourceIdCache.clear();
    directTextureCache.clear();
    imgResourceRowCache.clear();
    resourceResolutionCache.clear();
    archiveNameOverrides.clear();
    masterResourceNameOverrides.clear();
}

bool StorylandArchiveBrowser::readWholeFile(const std::wstring& path, std::vector<uint8_t>& outBytes, std::string& errorMessage) const {
    FILE* file = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr) {
        errorMessage = "Could not open file.";
        return false;
    }
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        fclose(file);
        errorMessage = "Could not seek file.";
        return false;
    }
    const __int64 signedFileSize = _ftelli64(file);
    if (signedFileSize < 0 || _fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file);
        errorMessage = "Could not read file size.";
        return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(signedFileSize);
#else
    file = fopen(std::filesystem::path(path).u8string().c_str(), "rb");
    if (file == nullptr) {
        errorMessage = "Could not open file.";
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        errorMessage = "Could not seek file.";
        return false;
    }
    const long signedFileSize = ftell(file);
    if (signedFileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        errorMessage = "Could not read file size.";
        return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(signedFileSize);
#endif

    if (fileSize > STORYLAND_MAX_ARCHIVE_FILE_BYTES || fileSize > uint64_t((std::numeric_limits<size_t>::max)())) {
        fclose(file);
        errorMessage = "File is too large to load safely.";
        return false;
    }

    outBytes.assign(static_cast<size_t>(fileSize), uint8_t(0));
    if (!outBytes.empty()) {
        const size_t readCount = fread(outBytes.data(), 1, outBytes.size(), file);
        if (readCount != outBytes.size()) {
            fclose(file);
            outBytes.clear();
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
    FILE* file = fopen(std::filesystem::path(path).u8string().c_str(), "wb");
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
    if (packed.empty()) {
        errorMessage = "LVZ zlib stream is empty.";
        return false;
    }

    z_stream stream = {};
    int status = inflateInit(&stream);
    if (status != Z_OK) {
        errorMessage = "zlib inflateInit failed for LVZ.";
        return false;
    }

    std::array<uint8_t, 65536> temp{};
    size_t inputOffset = 0;
    while (status != Z_STREAM_END) {
        if (stream.avail_in == 0u && inputOffset < packed.size()) {
            const size_t feed = std::min<size_t>(packed.size() - inputOffset, (std::numeric_limits<uInt>::max)());
            stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(packed.data() + inputOffset));
            stream.avail_in = static_cast<uInt>(feed);
            inputOffset += feed;
        }

        stream.next_out = reinterpret_cast<Bytef*>(temp.data());
        stream.avail_out = static_cast<uInt>(temp.size());
        const uLong previousIn = stream.total_in;
        const uLong previousOut = stream.total_out;
        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            unpacked.clear();
            errorMessage = "Could not inflate LVZ zlib stream.";
            return false;
        }

        const size_t produced = temp.size() - static_cast<size_t>(stream.avail_out);
        if (produced != 0u) {
            if (produced > STORYLAND_MAX_LVZ_INFLATED_BYTES ||
                unpacked.size() > STORYLAND_MAX_LVZ_INFLATED_BYTES - produced) {
                inflateEnd(&stream);
                unpacked.clear();
                errorMessage = "Inflated LVZ exceeds the safe 1 GiB limit.";
                return false;
            }
            unpacked.insert(unpacked.end(), temp.data(), temp.data() + produced);
        }

        const bool progressed = stream.total_in != previousIn || stream.total_out != previousOut;
        if (status != Z_STREAM_END && !progressed && stream.avail_in == 0u && inputOffset >= packed.size()) {
            inflateEnd(&stream);
            unpacked.clear();
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
    if (unpacked.empty()) {
        errorMessage = "LVZ input is empty.";
        return false;
    }
    if (unpacked.size() > STORYLAND_MAX_LVZ_INFLATED_BYTES) {
        errorMessage = "LVZ input exceeds the safe 1 GiB limit.";
        return false;
    }

    z_stream stream = {};
    int status = deflateInit(&stream, Z_BEST_COMPRESSION);
    if (status != Z_OK) {
        errorMessage = "zlib deflateInit failed for LVZ.";
        return false;
    }

    std::array<uint8_t, 65536> temp{};
    size_t inputOffset = 0;
    bool finishing = false;
    while (status != Z_STREAM_END) {
        if (!finishing && stream.avail_in == 0u && inputOffset < unpacked.size()) {
            const size_t feed = std::min<size_t>(unpacked.size() - inputOffset, (std::numeric_limits<uInt>::max)());
            stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(unpacked.data() + inputOffset));
            stream.avail_in = static_cast<uInt>(feed);
            inputOffset += feed;
        }
        if (stream.avail_in == 0u && inputOffset >= unpacked.size()) finishing = true;

        stream.next_out = reinterpret_cast<Bytef*>(temp.data());
        stream.avail_out = static_cast<uInt>(temp.size());
        status = deflate(&stream, finishing ? Z_FINISH : Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            deflateEnd(&stream);
            packed.clear();
            errorMessage = "Could not deflate LVZ zlib stream.";
            return false;
        }

        const size_t produced = temp.size() - static_cast<size_t>(stream.avail_out);
        if (produced != 0u) packed.insert(packed.end(), temp.data(), temp.data() + produced);
        if (packed.size() > STORYLAND_MAX_ARCHIVE_FILE_BYTES) {
            deflateEnd(&stream);
            packed.clear();
            errorMessage = "Compressed LVZ exceeds the safe file limit.";
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

    std::vector<std::pair<uint32_t, uint32_t>> textureSectorRows;
    const bool haveTextureSectorRows = readSectorRowsFromLvz(currentLvzBytes, textureSectorRows);
    const int textureGameRows = haveTextureSectorRows ? detectWorldGameRowCount(textureSectorRows.size()) : 37;
    const char* standaloneTextureExtension = textureGameRows == 47 ? ".chk" : ".xtx";

    std::set<uint64_t> seenHeaders;
    std::set<std::pair<uint64_t, int32_t>> seenBindings;
    std::vector<std::pair<int32_t, uint32_t>> legacyTextureReferences;

    auto appendTexture = [&](const std::vector<uint8_t>& bytes, size_t offset, size_t baseStart, size_t baseEnd, const char* prefix, int32_t materialId, const char* source) {
        uint64_t key = (uint64_t(baseStart & 0xFFFFFFFFu) << 32) | uint64_t(offset & 0xFFFFFFFFu);
        if (materialId >= 0) {
            if (seenBindings.find({key, materialId}) != seenBindings.end()) return;
        } else if (seenHeaders.find(key) != seenHeaders.end()) {
            return;
        }

        StorylandDirectTextureResource texture;
        if (!decodeDirectTexture(bytes, offset, baseStart, baseEnd, texture)) return;

        texture.index = uint32_t(directTextureCache.size());
        texture.materialId = materialId;
        texture.storedInImg = (&bytes == &currentImgBytes);
        texture.baseOffset = uint32_t(baseStart);
        const size_t pixelCount = size_t(texture.width) * size_t(texture.height);
        const size_t rasterBytes = texture.bpp == 4 ? (pixelCount + 1u) / 2u :
                                   texture.bpp == 8 ? pixelCount : pixelCount * 4u;
        const size_t paletteBytes = texture.bpp == 4 ? 64u : texture.bpp == 8 ? 1024u : 0u;
        texture.storageBytes = uint32_t(std::min<size_t>(uint32_t(-1), rasterBytes + paletteBytes));
        texture.source = source != nullptr ? source : "scan";
        if (materialId >= 0) texture.name = "texture" + std::to_string(materialId) + standaloneTextureExtension;
        else texture.name = "texture" + std::to_string(texture.index) + standaloneTextureExtension;

        directTextureCache.push_back(std::move(texture));
        seenHeaders.insert(key);
        if (materialId >= 0) seenBindings.insert({key, materialId});
    };

    // Retail master WRLD Resource[] recovery.  The Resource[] count is stored
    // at WRLD +0x14C; +0x14 is the relocation-entry count and must not be used
    // as a resource count.
    StorylandMasterResourceTableInfo masterTableInfo;
    if (locateMasterResourceTable(currentLvzBytes, masterTableInfo)) {
        const uint32_t table = masterTableInfo.tableOffset;
        const uint32_t count = masterTableInfo.count;
        const uint32_t stride = masterTableInfo.stride;

        for (uint32_t i = 0; i < count; ++i) {
            const size_t row = size_t(table) + size_t(i) * stride;
            const uint32_t pointer = readU32(currentLvzBytes, row + 0u);
            if (pointer < 0x40u || uint64_t(pointer) + 16ull > masterTableInfo.dataEnd) continue;

            appendTexture(currentLvzBytes, pointer, 0, masterTableInfo.dataEnd,
                          "lvz_res_texture", int32_t(i), "master Resource[]");

            // TEX_REF rows point at a shared texture record and retain the
            // referring Resource[] index as the material binding id.
            const uint32_t referenced = readU32(currentLvzBytes, pointer);
            if (referenced >= 0x40u && uint64_t(referenced) + 16ull <= masterTableInfo.dataEnd) {
                if (looksLikeDirectLvzTextureCandidate(currentLvzBytes, referenced, 0, masterTableInfo.dataEnd)) {
                    appendTexture(currentLvzBytes, referenced, 0, masterTableInfo.dataEnd,
                                  "lvz_ref_texture", int32_t(i), "master Resource[] TEX_REF");
                } else {
                    legacyTextureReferences.push_back({int32_t(i), referenced});
                }
            }
        }
    }

    if (!legacyTextureReferences.empty()) {
        std::vector<uint32_t> starts;
        for (const auto& reference : legacyTextureReferences) starts.push_back(reference.second);
        std::sort(starts.begin(), starts.end());
        starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
        for (const auto& reference : legacyTextureReferences) {
            uint32_t materialId = uint32_t(reference.first);
            uint32_t start = reference.second;
            auto next = std::upper_bound(starts.begin(), starts.end(), start);
            size_t end = next == starts.end() ? currentLvzBytes.size() : size_t(*next);
            StorylandDirectTextureResource texture;
            if (!decodeLegacyTextureReference(currentLvzBytes, start, end, texture)) continue;
            texture.index = uint32_t(directTextureCache.size());
            texture.materialId = int32_t(materialId);
            texture.storedInImg = false;
            texture.baseOffset = 0u;
            texture.storageBytes = uint32_t((size_t(texture.width) * size_t(texture.height) + 1u) / 2u + 64u);
            texture.source = "master Resource[] TEX_REF legacy 4bpp";
            texture.name = "texture" + std::to_string(materialId) + standaloneTextureExtension;
            directTextureCache.push_back(std::move(texture));
        }
    }

    // Bind direct textures stored in official AreaInfo[] -> AERA Resource[]
    // rows.  A raw AREA scan can decode their pixels, but only this table keeps
    // the RES id required by world-mesh materials.
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> bestTextureAreas;
    int bestTextureAreaScore = -1;
    for (size_t pairOffset = 0x150; pairOffset + 8 <= std::min<size_t>(currentLvzBytes.size(), 0x508); pairOffset += 4) {
        uint32_t count = readU32(currentLvzBytes, pairOffset);
        uint32_t table = readU32(currentLvzBytes, pairOffset + 4);
        if (count == 0 || count > 1024 || table < 0x20 || uint64_t(table) + uint64_t(count) * 16ull > currentLvzBytes.size()) continue;
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> areas;
        for (uint32_t i = 0; i < count; ++i) {
            size_t row = size_t(table) + size_t(i) * 16u;
            uint32_t base = readU32(currentLvzBytes, row + 4);
            uint32_t fileSize = readU32(currentLvzBytes, row + 8);
            if (fileSize < 0x28 || uint64_t(base) + fileSize > currentImgBytes.size()) continue;
            if (!isAreaIdent(readU32(currentImgBytes, base))) continue;
            areas.emplace_back(i, base, fileSize);
        }
        if (areas.size() < std::min<size_t>(count, 3u)) continue;
        int score = int(areas.size() * 1000u) - std::abs(int(count) - int(areas.size()));
        if (pairOffset == 0x2F0) score += 100;
        if (score > bestTextureAreaScore) { bestTextureAreaScore = score; bestTextureAreas = std::move(areas); }
    }
    for (const auto& area : bestTextureAreas) {
        uint32_t areaIndex, base, fileSize;
        std::tie(areaIndex, base, fileSize) = area;
        int32_t resourceCount = readI32(currentImgBytes, size_t(base) + 0x20);
        uint32_t resourcePointer = readU32(currentImgBytes, size_t(base) + 0x24);
        if (resourceCount <= 0 || resourceCount > 4096) continue;
        uint64_t table = uint64_t(base) + resourcePointer;
        uint32_t reloc = readU32(currentImgBytes, size_t(base) + 0x10);
        size_t dataEnd = size_t(base) + ((reloc >= 0x20 && reloc <= fileSize) ? reloc : fileSize);
        dataEnd = std::min(dataEnd, currentImgBytes.size());
        if (table + uint64_t(resourceCount) * 8ull > uint64_t(base) + fileSize) continue;
        for (int32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
            size_t row = size_t(table) + size_t(rowIndex) * 8u;
            int16_t resourceId = readI16(currentImgBytes, row);
            uint32_t pointer = readU32(currentImgBytes, row + 4);
            if (resourceId < 0 || pointer < 0x20) continue;
            size_t rawOffset = size_t(base) + pointer;
            if (rawOffset + 16 > dataEnd) continue;
            char prefix[64] = {};
            std::snprintf(prefix, sizeof(prefix), "aera_%04u_RES_%d_texture", areaIndex, int(resourceId));
            appendTexture(currentImgBytes, rawOffset, base, dataEnd, prefix, resourceId, "official AERA Resource[]");
        }
    }

    for (size_t offset = 0x40; offset + 80 <= currentLvzBytes.size(); offset += 4) {
        if (readU32(currentLvzBytes, offset) != 0xCCCCCCCCu) continue;
        appendTexture(currentLvzBytes, offset, 0, currentLvzBytes.size(), "lvz_direct_texture", -1, "unbound LVZ scan");
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
            appendTexture(currentImgBytes, offset, baseStart, baseEnd, prefix, -1, "unbound AREA scan");
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

    std::set<std::array<uint32_t, 16>> seenPlacements;
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

                    // An IPL id is not unique.  Retail rows repeat across the
                    // sectors their sphere overlaps, but distinct instances can
                    // share IPL+RES+pass.  Match BLeeds by including the stable
                    // world sphere and full 3x3 basis in the dedupe identity.
                    std::array<uint32_t, 16> dedupeKey = {};
                    dedupeKey[0] = iplId;
                    dedupeKey[1] = resourceId;
                    dedupeKey[2] = uint32_t(passIndex);
                    dedupeKey[3] = readU32(currentImgBytes, rowOffset + 0x04);
                    dedupeKey[4] = readU32(currentImgBytes, rowOffset + 0x08);
                    dedupeKey[5] = readU32(currentImgBytes, rowOffset + 0x10 + 0x00);
                    dedupeKey[6] = readU32(currentImgBytes, rowOffset + 0x10 + 0x04);
                    dedupeKey[7] = readU32(currentImgBytes, rowOffset + 0x10 + 0x08);
                    dedupeKey[8] = readU32(currentImgBytes, rowOffset + 0x10 + 0x10);
                    dedupeKey[9] = readU32(currentImgBytes, rowOffset + 0x10 + 0x14);
                    dedupeKey[10] = readU32(currentImgBytes, rowOffset + 0x10 + 0x18);
                    dedupeKey[11] = readU32(currentImgBytes, rowOffset + 0x10 + 0x20);
                    dedupeKey[12] = readU32(currentImgBytes, rowOffset + 0x10 + 0x24);
                    dedupeKey[13] = readU32(currentImgBytes, rowOffset + 0x10 + 0x28);
                    if (!seenPlacements.insert(dedupeKey).second) {
                        rowOffset64 += 0x50ull;
                        continue;
                    }

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
    masterMeshResourceIdCache.clear();
    imgResourceRowCache.clear();
    resourceResolutionCache.clear();
    if (currentImgBytes.empty() || currentLvzBytes.empty()) return;

    std::set<uint64_t> neededKeys;
    for (const StorylandWorldPlacement& placement : worldPlacements) {
        uint64_t key = (uint64_t(placement.sectorIndex) << 32) | uint64_t(placement.resourceIndex);
        neededKeys.insert(key);
    }
    std::set<uint32_t> neededResourceIds;
    std::map<uint64_t, uint32_t> placementCounts;
    std::map<uint64_t, const StorylandWorldPlacement*> firstPlacementByKey;
    for (const StorylandWorldPlacement& placement : worldPlacements) {
        neededResourceIds.insert(placement.resourceIndex);
        uint64_t key = (uint64_t(placement.sectorIndex) << 32) | uint64_t(placement.resourceIndex);
        placementCounts[key]++;
        if (firstPlacementByKey.find(key) == firstPlacementByKey.end()) firstPlacementByKey[key] = &placement;
    }
    for (const auto& namedResource : masterResourceNameOverrides) {
        neededResourceIds.insert(namedResource.first);
    }
    if (neededResourceIds.empty()) return;

    struct CandidateMesh {
        StorylandWorldMesh mesh;
        uint64_t payloadOffset = 0;
        std::string source;
    };
    std::map<uint64_t, CandidateMesh> exactMeshes;
    std::map<uint32_t, std::vector<CandidateMesh>> meshesByResource;
    std::map<uint64_t, std::vector<CandidateMesh>> meshesByRowResource;
    std::map<uint32_t, std::vector<CandidateMesh>> officialAreaMeshes;
    std::map<uint32_t, std::vector<CandidateMesh>> masterLvzMeshes;
    std::map<uint32_t, std::vector<CandidateMesh>> continuationMeshes;
    std::set<std::pair<uint32_t, uint64_t>> parsedPayloads;

    auto rememberCandidate = [&](uint32_t sectorIndex, uint32_t sectorY, uint32_t resourceId,
                                 uint64_t rawOffset, uint64_t maxEnd, const std::string& source,
                                 bool sameSector, StorylandImgResourceRow* record) {
        if (neededResourceIds.find(resourceId) == neededResourceIds.end()) return;
        if (rawOffset + 4 > currentImgBytes.size() || maxEnd <= rawOffset + 4) return;
        if (!parsedPayloads.insert({resourceId, rawOffset}).second) return;
        StorylandWorldMesh mesh;
        size_t descriptorOffset = size_t(rawOffset);
        if (!parseWorldOverlayMeshNear(currentImgBytes, size_t(rawOffset), size_t(maxEnd), sectorIndex, resourceId, mesh, descriptorOffset) ||
            mesh.vertices.empty() || mesh.triangles.empty()) return;
        mesh.rawOffset = descriptorOffset;
        CandidateMesh candidate{std::move(mesh), uint64_t(descriptorOffset), source};
        if (record != nullptr) {
            record->decodedAsMesh = true;
            record->payloadOffset = descriptorOffset;
        }
        uint64_t key = (uint64_t(sectorIndex) << 32) | uint64_t(resourceId);
        if (sameSector && exactMeshes.find(key) == exactMeshes.end()) exactMeshes.emplace(key, candidate);
        uint64_t rowKey = (uint64_t(sectorY) << 32) | uint64_t(resourceId);
        meshesByRowResource[rowKey].push_back(candidate);
        meshesByResource[resourceId].push_back(candidate);
    };

    std::vector<uint64_t> containerStarts;
    for (const auto& sector : worldSectors) containerStarts.push_back(sector.imgOffset);
    std::sort(containerStarts.begin(), containerStarts.end());
    containerStarts.erase(std::unique(containerStarts.begin(), containerStarts.end()), containerStarts.end());

    for (const StorylandWorldSector& sector : worldSectors) {
        uint64_t cont = sector.imgOffset;
        uint64_t declaredEnd = std::min<uint64_t>(currentImgBytes.size(), sector.imgOffset + sector.byteSize);
        auto nextContainer = std::upper_bound(containerStarts.begin(), containerStarts.end(), cont);
        uint64_t hardEnd = nextContainer == containerStarts.end() ? currentImgBytes.size() : *nextContainer;
        uint64_t end = std::max<uint64_t>(declaredEnd, std::min<uint64_t>(currentImgBytes.size(), hardEnd));
        if (cont + 8 > end) continue;

        uint32_t resourcesPointer = readU32(currentImgBytes, size_t(cont) + 0x00);
        uint32_t resourceCount = readU16(currentImgBytes, size_t(cont) + 0x04);
        if (resourceCount == 0 || resourceCount > 4096) continue;

        uint64_t listStart = cont + uint64_t(resourcesPointer) - 0x20ull;
        if (listStart < cont || listStart + 8ull > end) continue;

        auto processRow = [&](uint32_t rowIndex, uint32_t resourceId, uint32_t rawPointer,
                              uint64_t rowOffset, const char* layout) {
            if (neededResourceIds.find(resourceId) == neededResourceIds.end()) return;
            uint64_t key = (uint64_t(sector.sectorIndex) << 32) | uint64_t(resourceId);
            std::vector<uint64_t> offsets;
            auto pushOffset = [&](int64_t value) {
                if (value < 0 || uint64_t(value) + 4 > currentImgBytes.size()) return;
                uint64_t offset = uint64_t(value);
                if ((offset & 3ull) != 0) return;
                if (std::find(offsets.begin(), offsets.end(), offset) == offsets.end()) offsets.push_back(offset);
            };
            pushOffset(int64_t(cont) + int64_t(rawPointer) - 0x20ll);
            pushOffset(int64_t(cont) + int64_t(rawPointer));
            pushOffset(int64_t(rawPointer));
            pushOffset(int64_t(rawPointer) - 0x20ll);

            StorylandImgResourceRow record;
            record.sectorIndex = sector.sectorIndex;
            record.sectorX = sector.sectorX;
            record.sectorY = sector.sectorY;
            record.rowIndex = rowIndex;
            record.resourceId = resourceId;
            record.tableOffset = rowOffset;
            record.layout = layout;
            record.usedByPlacement = neededKeys.find(key) != neededKeys.end();
            if (!offsets.empty()) record.payloadOffset = offsets.front();
            record.payloadSize = end > record.payloadOffset ? end - record.payloadOffset : 0;

            for (uint64_t offset : offsets) {
                uint64_t parseEnd = (offset >= cont && offset < end) ? end : currentImgBytes.size();
                size_t before = meshesByResource[resourceId].size();
                rememberCandidate(sector.sectorIndex, sector.sectorY, resourceId, offset, parseEnd,
                                  std::string("sector ") + layout, true, &record);
                if (meshesByResource[resourceId].size() > before) break;
            }
            imgResourceRowCache.push_back(std::move(record));
        };

        // LCS layout: s32 RES, u32 pointer.
        if (listStart + uint64_t(resourceCount) * 8ull <= end) {
            for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
                uint64_t row = listStart + uint64_t(rowIndex) * 8ull;
                int32_t id = readI32(currentImgBytes, size_t(row));
                if (id < 0) continue;
                processRow(rowIndex, uint32_t(id), readU32(currentImgBytes, size_t(row) + 4), row, "id_ptr/8");
            }
        }

        // VCS layout: u32 pointer, u32 flags/unused, u32 RES.
        if (listStart + uint64_t(resourceCount) * 12ull <= end) {
            for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
                uint64_t row = listStart + uint64_t(rowIndex) * 12ull;
                uint32_t a = readU32(currentImgBytes, size_t(row));
                uint32_t b = readU32(currentImgBytes, size_t(row) + 4);
                uint32_t c = readU32(currentImgBytes, size_t(row) + 8);
                processRow(rowIndex, c, a, row, "ptr_unused_id/12");
                // Targeted variants used by several VCS AREA/mainland tables.
                processRow(rowIndex, b, a, row, "ptr_id_unused/12");
                processRow(rowIndex, a, c, row, "id_unused_ptr/12");
                processRow(rowIndex, a, b, row, "id_ptr_unused/12");
            }
        }
    }

    // Master WRLD Resource[] contains normal model payloads.  Parse the IDs
    // required by placements plus any resources added during this editing
    // session.  The table is the retail 12-byte {pointer, unknown, id} form.
    StorylandMasterResourceTableInfo masterTableInfo;
    if (locateMasterResourceTable(currentLvzBytes, masterTableInfo)) {
        std::vector<uint32_t> starts;
        starts.reserve(masterTableInfo.count);
        for (uint32_t i = 0; i < masterTableInfo.count; ++i) {
            const size_t row = size_t(masterTableInfo.tableOffset) + size_t(i) * masterTableInfo.stride;
            const uint32_t pointer = readU32(currentLvzBytes, row + 0u);
            if (pointer >= 0x40u && pointer < masterTableInfo.dataEnd) starts.push_back(pointer);
        }
        std::sort(starts.begin(), starts.end());
        starts.erase(std::unique(starts.begin(), starts.end()), starts.end());

        // Discover master model resources independently of world placement.  A
        // Resource[] model can legitimately exist without any sGeomInstance yet
        // (for example, a newly-added mod resource).  This lightweight probe
        // validates one material list and one Leeds VIF strip instead of fully
        // decoding every unplaced resource.
        for (uint32_t i = 0; i < masterTableInfo.count; ++i) {
            const size_t row = size_t(masterTableInfo.tableOffset) + size_t(i) * masterTableInfo.stride;
            const uint32_t pointer = readU32(currentLvzBytes, row + 0u);
            if (pointer < 0x40u || pointer >= masterTableInfo.dataEnd) continue;
            auto next = std::upper_bound(starts.begin(), starts.end(), pointer);
            const size_t payloadEnd = next == starts.end() ? size_t(masterTableInfo.dataEnd) : size_t(*next);
            if (payloadEnd <= pointer + 4u) continue;

            const size_t searchEnd = std::min(payloadEnd, size_t(pointer) + 0x180u);
            bool isMesh = false;
            for (size_t candidate = pointer; candidate + 4u <= searchEnd; candidate += 4u) {
                ParsedWorldMaterialList materialList;
                if (!parseWorldMaterialList(currentLvzBytes, candidate, payloadEnd, materialList)) continue;
                const size_t unpack = findUnpackNear(currentLvzBytes, materialList.streamStart, payloadEnd, 32u);
                if (unpack == SIZE_MAX) continue;
                ParsedWorldStrip firstStrip;
                if (!parseOneWorldVifStrip(currentLvzBytes, unpack, payloadEnd, firstStrip) || firstStrip.vertices.size() < 3u) continue;
                isMesh = true;
                break;
            }
            if (isMesh) masterMeshResourceIdCache.push_back(i);
        }

        for (uint32_t i : neededResourceIds) {
            if (i >= masterTableInfo.count) continue;
            const size_t row = size_t(masterTableInfo.tableOffset) + size_t(i) * masterTableInfo.stride;
            const uint32_t pointer = readU32(currentLvzBytes, row + 0u);
            if (pointer < 0x40u || pointer >= masterTableInfo.dataEnd) continue;

            auto next = std::upper_bound(starts.begin(), starts.end(), pointer);
            const size_t payloadEnd = next == starts.end() ? size_t(masterTableInfo.dataEnd) : size_t(*next);
            if (payloadEnd <= pointer) continue;

            StorylandWorldMesh mesh;
            size_t descriptorOffset = pointer;
            if (!parseWorldOverlayMeshNear(currentLvzBytes, pointer, payloadEnd, 0xFFFFFFFFu, i, mesh, descriptorOffset) ||
                mesh.vertices.empty() || mesh.triangles.empty()) continue;

            mesh.rawOffset = descriptorOffset;
            CandidateMesh candidate{std::move(mesh), uint64_t(descriptorOffset), "master LVZ"};
            masterLvzMeshes[i].push_back(candidate);
            meshesByResource[i].push_back(candidate);
        }
    }

    // Triggered/AREA and nested child WRLD headers reference additional IMG
    // containers which are not part of the 623 static sector grid.  They are
    // model sources only.  Scan proven relocatable headers and feed their exact
    // Resource[] rows into the same RES candidate pool used for placement fit.
    std::set<uint32_t> sectorHeaderOffsets;
    std::set<uint64_t> sectorContainerOffsets;
    for (const auto& sector : worldSectors) {
        sectorHeaderOffsets.insert(sector.headerOffset);
        sectorContainerOffsets.insert(sector.imgOffset);
    }
    std::set<std::pair<uint32_t, uint64_t>> seenExtraContainers;
    uint32_t extraContainerIndex = 0;
    std::vector<uint32_t> extraHeaderCandidates;
    size_t groupCursor = 0x24;
    while (groupCursor + 8 <= currentLvzBytes.size()) {
        uint32_t groupHeader = readU32(currentLvzBytes, groupCursor);
        if (groupHeader == 0 || (groupHeader & 3u) != 0 || uint64_t(groupHeader) + 0x20ull > currentLvzBytes.size()) break;
        uint32_t groupTag = readU32(currentLvzBytes, groupHeader);
        if (groupTag != WRLD_IDENT && groupTag != TEX_IDENT) break;
        uint32_t childCount = readU32(currentLvzBytes, size_t(groupHeader) + 0x14);
        if (childCount == 0 || childCount > 65536) childCount = 1;
        for (uint32_t child = 0; child < childCount; ++child) {
            uint64_t childHeader = uint64_t(groupHeader) + uint64_t(child) * 0x20ull;
            if (childHeader + 0x20ull > currentLvzBytes.size()) break;
            uint32_t childTag = readU32(currentLvzBytes, size_t(childHeader));
            if (childTag != WRLD_IDENT && childTag != TEX_IDENT) break;
            extraHeaderCandidates.push_back(uint32_t(childHeader));
        }
        groupCursor += 8;
    }
    std::sort(extraHeaderCandidates.begin(), extraHeaderCandidates.end());
    extraHeaderCandidates.erase(std::unique(extraHeaderCandidates.begin(), extraHeaderCandidates.end()), extraHeaderCandidates.end());

    for (uint32_t header : extraHeaderCandidates) {
        uint32_t tag = readU32(currentLvzBytes, header);
        if (tag != WRLD_IDENT && tag != TEX_IDENT) continue;
        if (sectorHeaderOffsets.find(uint32_t(header)) != sectorHeaderOffsets.end()) continue;
        uint32_t total = readU32(currentLvzBytes, header + 0x08);
        uint32_t cont32 = readU32(currentLvzBytes, header + 0x18);
        if (total < 0x20 || total > 0x04000000u || cont32 >= currentImgBytes.size()) continue;
        uint64_t cont = cont32;
        if (sectorContainerOffsets.find(cont) != sectorContainerOffsets.end()) continue;
        uint64_t end = std::min<uint64_t>(currentImgBytes.size(), cont + uint64_t(total - 0x20u));
        if (end <= cont + 8 || !seenExtraContainers.insert({uint32_t(header), cont}).second) continue;
        uint32_t resourcesPointer = readU32(currentImgBytes, size_t(cont));
        uint32_t resourceCount = readU16(currentImgBytes, size_t(cont) + 4);
        if (resourceCount == 0 || resourceCount > 4096 || resourcesPointer < 0x20) continue;
        uint64_t listStart = cont + resourcesPointer - 0x20ull;
        if (listStart < cont || listStart + 8 > end) continue;
        uint32_t syntheticSector = 0x90000000u + extraContainerIndex++;

        auto processExtraRow = [&](uint32_t rowIndex, uint32_t resourceId, uint32_t rawPointer,
                                   uint64_t rowOffset, const char* layout) {
            if (neededResourceIds.find(resourceId) == neededResourceIds.end()) return;
            std::vector<uint64_t> offsets;
            auto push = [&](int64_t value) {
                if (value < 0 || uint64_t(value) + 4 > currentImgBytes.size()) return;
                uint64_t offset = uint64_t(value);
                if ((offset & 3ull) == 0 && std::find(offsets.begin(), offsets.end(), offset) == offsets.end()) offsets.push_back(offset);
            };
            push(int64_t(cont) + int64_t(rawPointer) - 0x20ll);
            push(int64_t(cont) + int64_t(rawPointer));
            push(int64_t(rawPointer));
            push(int64_t(rawPointer) - 0x20ll);

            StorylandImgResourceRow record;
            record.sectorIndex = syntheticSector;
            record.rowIndex = rowIndex;
            record.resourceId = resourceId;
            record.tableOffset = rowOffset;
            record.layout = std::string("extra IMG ") + layout;
            record.usedByPlacement = true;
            if (!offsets.empty()) record.payloadOffset = offsets.front();
            record.payloadSize = end > record.payloadOffset ? end - record.payloadOffset : 0;
            for (uint64_t offset : offsets) {
                uint64_t parseEnd = offset >= cont && offset < end ? end : currentImgBytes.size();
                size_t before = meshesByResource[resourceId].size();
                rememberCandidate(syntheticSector, 0xFFFFFFFFu, resourceId, offset, parseEnd,
                                  std::string("extra IMG ") + layout, false, &record);
                if (meshesByResource[resourceId].size() > before) break;
            }
            imgResourceRowCache.push_back(std::move(record));
        };

        if (listStart + uint64_t(resourceCount) * 8ull <= end) {
            for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
                uint64_t row = listStart + uint64_t(rowIndex) * 8ull;
                int32_t id = readI32(currentImgBytes, size_t(row));
                if (id >= 0) processExtraRow(rowIndex, uint32_t(id), readU32(currentImgBytes, size_t(row) + 4), row, "id_ptr/8");
            }
        }
        if (listStart + uint64_t(resourceCount) * 12ull <= end) {
            for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
                uint64_t row = listStart + uint64_t(rowIndex) * 12ull;
                uint32_t a = readU32(currentImgBytes, size_t(row));
                uint32_t b = readU32(currentImgBytes, size_t(row) + 4);
                uint32_t c = readU32(currentImgBytes, size_t(row) + 8);
                processExtraRow(rowIndex, c, a, row, "ptr_unused_id/12");
                processExtraRow(rowIndex, b, a, row, "ptr_id_unused/12");
                processExtraRow(rowIndex, a, c, row, "id_unused_ptr/12");
                processExtraRow(rowIndex, a, b, row, "id_ptr_unused/12");
            }
        }
    }

    // Authoritative VCS AreaInfo[] -> AERA AreaResource[] path.  These rows are
    // s16 RES, s16 CBaseModelInfo id, u32 chunk-relative pointer and account for
    // many high-number streamed models absent from sector-local tables.
    std::vector<std::tuple<uint32_t, int16_t, int16_t, uint32_t, uint32_t, uint32_t>> bestAreas;
    int bestAreaScore = -1;
    for (size_t pairOffset = 0x150; pairOffset + 8 <= std::min<size_t>(currentLvzBytes.size(), 0x508); pairOffset += 4) {
        uint32_t count = readU32(currentLvzBytes, pairOffset);
        uint32_t table = readU32(currentLvzBytes, pairOffset + 4);
        if (count == 0 || count > 1024 || table < 0x20 || uint64_t(table) + uint64_t(count) * 16ull > currentLvzBytes.size()) continue;
        std::vector<std::tuple<uint32_t, int16_t, int16_t, uint32_t, uint32_t, uint32_t>> areas;
        for (uint32_t i = 0; i < count; ++i) {
            size_t row = size_t(table) + size_t(i) * 16u;
            int16_t cellX = readI16(currentLvzBytes, row);
            int16_t cellY = readI16(currentLvzBytes, row + 2);
            uint32_t fileOffset = readU32(currentLvzBytes, row + 4);
            uint32_t fileSize = readU32(currentLvzBytes, row + 8);
            uint32_t declaredResources = readU32(currentLvzBytes, row + 12);
            if (fileSize < 0x28 || uint64_t(fileOffset) + fileSize > currentImgBytes.size()) continue;
            if (!isAreaIdent(readU32(currentImgBytes, fileOffset))) continue;
            areas.emplace_back(i, cellX, cellY, fileOffset, fileSize, declaredResources);
        }
        if (areas.size() < std::min<size_t>(count, 3u)) continue;
        int score = int(areas.size() * 1000u) - std::abs(int(count) - int(areas.size()));
        if (pairOffset == 0x2F0) score += 100;
        if (score > bestAreaScore) { bestAreaScore = score; bestAreas = std::move(areas); }
    }

    for (const auto& area : bestAreas) {
        uint32_t areaIndex, base, fileSize, declaredResources;
        int16_t cellX, cellY;
        std::tie(areaIndex, cellX, cellY, base, fileSize, declaredResources) = area;
        int32_t resourceCount = readI32(currentImgBytes, size_t(base) + 0x20);
        uint32_t resourcesPointer = readU32(currentImgBytes, size_t(base) + 0x24);
        if (resourceCount <= 0 || resourceCount > 4096) continue;
        if (declaredResources <= 4096 && declaredResources != uint32_t(resourceCount)) {
            // Mismatch is diagnostic only; the validated AERA header is authoritative.
        }
        uint64_t table = uint64_t(base) + resourcesPointer;
        uint32_t reloc = readU32(currentImgBytes, size_t(base) + 0x10);
        uint64_t dataEnd = uint64_t(base) + ((reloc >= 0x20 && reloc <= fileSize) ? reloc : fileSize);
        dataEnd = std::min<uint64_t>(dataEnd, currentImgBytes.size());
        if (table < uint64_t(base) + 0x20ull || table + uint64_t(resourceCount) * 8ull > uint64_t(base) + fileSize) continue;

        for (int32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
            uint64_t row = table + uint64_t(rowIndex) * 8ull;
            int16_t resourceId = readI16(currentImgBytes, size_t(row));
            int16_t secondaryId = readI16(currentImgBytes, size_t(row) + 2);
            uint32_t pointer = readU32(currentImgBytes, size_t(row) + 4);
            if (resourceId < 0 || neededResourceIds.find(uint32_t(resourceId)) == neededResourceIds.end()) continue;
            uint64_t rawOffset = uint64_t(base) + pointer;
            if (pointer < 0x20 || rawOffset + 8 > dataEnd) continue;

            StorylandImgResourceRow record;
            record.sectorIndex = 0x80000000u + areaIndex;
            record.sectorX = uint32_t(int32_t(cellX));
            record.sectorY = uint32_t(int32_t(cellY));
            record.rowIndex = uint32_t(rowIndex);
            record.resourceId = uint32_t(resourceId);
            record.secondaryId = secondaryId;
            record.tableOffset = row;
            record.payloadOffset = rawOffset;
            record.payloadSize = dataEnd - rawOffset;
            record.layout = "AERA s16_RES/s16_modelInfo/u32_ptr";
            record.usedByPlacement = true;

            size_t before = meshesByResource[uint32_t(resourceId)].size();
            rememberCandidate(record.sectorIndex, uint32_t(int32_t(cellY)), uint32_t(resourceId), rawOffset, dataEnd,
                              "official AERA", false, &record);
            if (meshesByResource[uint32_t(resourceId)].size() > before) {
                officialAreaMeshes[uint32_t(resourceId)].push_back(meshesByResource[uint32_t(resourceId)].back());
            }
            imgResourceRowCache.push_back(std::move(record));
        }
    }

    // EMPTY master resources can continue as headerless material+VIF streams
    // in the IMG.  Probe only strict aligned descriptor tables and only for RES
    // ids that still have no structured candidate.  Material ids RES/RES+1 are
    // the Leeds proof; placement bounds make the final selection authoritative.
    std::set<uint32_t> continuationWanted;
    for (uint32_t resourceId : neededResourceIds) {
        auto found = meshesByResource.find(resourceId);
        if (found == meshesByResource.end() || found->second.empty()) continuationWanted.insert(resourceId);
    }
    if (!continuationWanted.empty()) {
        for (size_t offset = 0x40; offset + 0x20 <= currentImgBytes.size(); offset += 0x10) {
            uint32_t count = readU16(currentImgBytes, offset);
            uint32_t sizeBytes = readU16(currentImgBytes, offset + 2);
            if (count == 0 || count > 256 || sizeBytes > 0x8000) continue;
            // Equivalent to ((4 + count*row + 15) & ~15) - 4.
            uint32_t expected24 = ((4u + count * 24u + 15u) & ~15u) - 4u;
            uint32_t expected22 = ((4u + count * 22u + 15u) & ~15u) - 4u;
            uint32_t rowLength = sizeBytes == expected24 ? 24u : sizeBytes == expected22 ? 22u : 0u;
            if (rowLength == 0 || offset + 4u + sizeBytes > currentImgBytes.size()) continue;

            size_t stream = offset + 4u + sizeBytes;
            size_t padding = 0;
            while (stream < currentImgBytes.size() && currentImgBytes[stream] == 0xAA && padding < 0x400) { stream++; padding++; }
            stream = alignUp4Size(stream);
            if (stream + 4 > currentImgBytes.size() || readU32(currentImgBytes, stream) != 0x6C018000u) continue;

            uint64_t packetTotal = 0;
            std::set<uint32_t> textureIds;
            bool valid = true;
            size_t row = offset + 4;
            for (uint32_t i = 0; i < count; ++i, row += rowLength) {
                uint32_t packetSize = 0;
                uint32_t textureId = 0;
                if (rowLength == 24) {
                    packetSize = readU32(currentImgBytes, row) >> 1;
                    textureId = readU16(currentImgBytes, row + 4);
                } else {
                    textureId = readU16(currentImgBytes, row);
                    packetSize = readU16(currentImgBytes, row + 2) & 0x7FFFu;
                }
                if (packetSize == 0 || packetSize > 0x40000u) { valid = false; break; }
                packetTotal += packetSize;
                if (packetTotal > 0x2000000ull) { valid = false; break; }
                textureIds.insert(textureId);
            }
            size_t packetEnd = stream + size_t(packetTotal);
            if (!valid || packetEnd > currentImgBytes.size()) continue;

            std::set<uint32_t> targets;
            for (uint32_t textureId : textureIds) {
                if (continuationWanted.find(textureId) != continuationWanted.end()) targets.insert(textureId);
                if (textureId > 0 && continuationWanted.find(textureId - 1u) != continuationWanted.end()) targets.insert(textureId - 1u);
            }
            for (uint32_t target : targets) {
                StorylandWorldMesh mesh;
                size_t descriptorOffset = offset;
                if (!parseWorldOverlayMeshNear(currentImgBytes, offset, packetEnd, 0xA0000000u, target, mesh, descriptorOffset) ||
                    mesh.vertices.empty() || mesh.triangles.empty()) continue;
                mesh.rawOffset = descriptorOffset;
                CandidateMesh candidate{std::move(mesh), uint64_t(descriptorOffset), "IMG continuation"};
                continuationMeshes[target].push_back(candidate);
                if (continuationMeshes[target].size() > 6) continuationMeshes[target].erase(continuationMeshes[target].begin() + 6, continuationMeshes[target].end());
            }
        }
    }

    // Resolve each used (sector, RES) key exactly.  A unique linked-sector
    // payload is safe to reuse; ambiguous same-RES payloads remain conflicts.
    for (const auto& count : placementCounts) {
        uint32_t sectorIndex = uint32_t(count.first >> 32);
        uint32_t resourceId = uint32_t(count.first & 0xFFFFFFFFu);
        StorylandResourceResolution resolution;
        resolution.sectorIndex = sectorIndex;
        resolution.resourceId = resourceId;
        resolution.placementCount = count.second;
        if (sectorIndex < worldSectors.size()) {
            resolution.sectorX = worldSectors[sectorIndex].sectorX;
            resolution.sectorY = worldSectors[sectorIndex].sectorY;
        }

        auto official = officialAreaMeshes.find(resourceId);
        auto master = masterLvzMeshes.find(resourceId);
        auto continuation = continuationMeshes.find(resourceId);
        auto exact = exactMeshes.find(count.first);
        uint64_t rowKey = (uint64_t(resolution.sectorY) << 32) | uint64_t(resourceId);
        auto rowCandidates = meshesByRowResource.find(rowKey);
        auto placementFitScore = [&](const CandidateMesh& candidate, const StorylandWorldPlacement& placement,
                                     double& outRadiusRatio, double& outCenterError) -> double {
            outRadiusRatio = std::numeric_limits<double>::infinity();
            outCenterError = std::numeric_limits<double>::infinity();
            if (candidate.mesh.vertices.empty()) return std::numeric_limits<double>::infinity();

            float minX = candidate.mesh.vertices.front().x;
            float maxX = minX;
            float minY = candidate.mesh.vertices.front().y;
            float maxY = minY;
            float minZ = candidate.mesh.vertices.front().z;
            float maxZ = minZ;
            for (const auto& vertex : candidate.mesh.vertices) {
                minX = std::min(minX, vertex.x); maxX = std::max(maxX, vertex.x);
                minY = std::min(minY, vertex.y); maxY = std::max(maxY, vertex.y);
                minZ = std::min(minZ, vertex.z); maxZ = std::max(maxZ, vertex.z);
            }

            double wx0 = std::numeric_limits<double>::infinity();
            double wy0 = wx0;
            double wz0 = wx0;
            double wx1 = -wx0;
            double wy1 = -wx0;
            double wz1 = -wx0;
            for (float x : {minX, maxX}) {
                for (float y : {minY, maxY}) {
                    for (float z : {minZ, maxZ}) {
                        const double wx = placement.matrix[0] * x + placement.matrix[4] * y + placement.matrix[8] * z + placement.matrix[12];
                        const double wy = placement.matrix[1] * x + placement.matrix[5] * y + placement.matrix[9] * z + placement.matrix[13];
                        const double wz = placement.matrix[2] * x + placement.matrix[6] * y + placement.matrix[10] * z + placement.matrix[14];
                        wx0 = std::min(wx0, wx); wx1 = std::max(wx1, wx);
                        wy0 = std::min(wy0, wy); wy1 = std::max(wy1, wy);
                        wz0 = std::min(wz0, wz); wz1 = std::max(wz1, wz);
                    }
                }
            }

            const double cx = (wx0 + wx1) * 0.5;
            const double cy = (wy0 + wy1) * 0.5;
            const double cz = (wz0 + wz1) * 0.5;
            const double radius = std::sqrt((wx1 - cx) * (wx1 - cx) +
                                            (wy1 - cy) * (wy1 - cy) +
                                            (wz1 - cz) * (wz1 - cz));
            const double targetRadius = std::max(0.0001, double(std::fabs(placement.boundRadius)));
            outCenterError = std::sqrt((cx - placement.boundX) * (cx - placement.boundX) +
                                       (cy - placement.boundY) * (cy - placement.boundY) +
                                       (cz - placement.boundZ) * (cz - placement.boundZ));
            outRadiusRatio = radius / targetRadius;
            if (!std::isfinite(outCenterError) || !std::isfinite(outRadiusRatio) || outRadiusRatio <= 0.0) {
                return std::numeric_limits<double>::infinity();
            }
            return outCenterError / targetRadius + std::fabs(std::log(outRadiusRatio));
        };

        auto candidateFitsPlacement = [&](const CandidateMesh& candidate, const StorylandWorldPlacement& placement,
                                          double* outScore = nullptr) -> bool {
            double radiusRatio = 0.0;
            double centerError = 0.0;
            const double score = placementFitScore(candidate, placement, radiusRatio, centerError);
            if (outScore != nullptr) *outScore = score;
            if (!std::isfinite(score)) return false;

            const double targetRadius = std::max(0.0001, double(std::fabs(placement.boundRadius)));
            const double normalizedCenterError = centerError / targetRadius;

            // Retail WRLD placements carry a world-space bounding sphere.  A
            // decoded payload which becomes only a tiny fraction of that sphere,
            // or explodes far beyond it, is almost certainly a false mesh parse
            // or a RES collision from another container.  This is especially
            // visible on VCS UNDERWATER/seabed resources because their legitimate
            // matrices intentionally use large 256/512 basis scales.
            if (radiusRatio < 0.18 || radiusRatio > 2.75) return false;
            if (normalizedCenterError > 1.75) return false;
            return score <= 2.65;
        };

        auto chooseByPlacement = [&](const std::vector<CandidateMesh>& candidates) -> const CandidateMesh* {
            if (candidates.empty()) return nullptr;
            auto placementIt = firstPlacementByKey.find(count.first);
            if (placementIt == firstPlacementByKey.end() || placementIt->second == nullptr) return nullptr;
            const StorylandWorldPlacement& placement = *placementIt->second;
            const CandidateMesh* best = nullptr;
            double bestScore = std::numeric_limits<double>::infinity();
            for (const CandidateMesh& candidate : candidates) {
                double score = std::numeric_limits<double>::infinity();
                if (!candidateFitsPlacement(candidate, placement, &score)) continue;
                if (score < bestScore) {
                    bestScore = score;
                    best = &candidate;
                }
            }
            return best;
        };

        const CandidateMesh* officialChoice = official == officialAreaMeshes.end() ? nullptr : chooseByPlacement(official->second);
        if (officialChoice == nullptr && official != officialAreaMeshes.end() && official->second.size() == 1u) {
            // AreaInfo[] -> AERA is an authoritative streamed-resource mapping.
            // A single valid AERA payload is deterministic even when the retail
            // placement sphere is conservative or uses a different LOD bound.
            officialChoice = &official->second.front();
        }
        if (officialChoice != nullptr) {
            resolution.source = "official AERA";
            resolution.candidateCount = uint32_t(official->second.size());
            resolution.payloadOffset = officialChoice->payloadOffset;
            StorylandWorldMesh linked = officialChoice->mesh;
            linked.sectorIndex = sectorIndex;
            worldMeshCache.push_back(std::move(linked));
        } else if (exact != exactMeshes.end()) {
            // A sector-local Resource[] payload is already the strongest mapping
            // available in the retail WRLD layout.  Placement bounds are useful as
            // a sanity check, but they are not authoritative enough to reject an
            // otherwise structurally valid exact (sector, RES) payload.  Bounds in
            // retail files can be conservative, shared, or scaled very differently
            // from the decoded strip geometry.
            resolution.candidateCount = 1;
            resolution.source = "same-sector";
            resolution.payloadOffset = exact->second.payloadOffset;
            StorylandWorldMesh linked = exact->second.mesh;
            linked.sectorIndex = sectorIndex;
            worldMeshCache.push_back(std::move(linked));
        } else if (rowCandidates != meshesByRowResource.end() && !rowCandidates->second.empty()) {
            resolution.candidateCount = uint32_t(rowCandidates->second.size());
            const CandidateMesh* choice = chooseByPlacement(rowCandidates->second);
            if (choice == nullptr && rowCandidates->second.size() == 1u) {
                // A single same-row candidate is deterministic.  Do not manufacture
                // a Missing/Conflict state merely because its retail placement sphere
                // is a poor geometric fit for Storyland's decoded bounds.
                choice = &rowCandidates->second.front();
            }
            if (choice != nullptr) {
                resolution.source = rowCandidates->second.size() == 1u ? "same-row" : "same-row verified";
                resolution.payloadOffset = choice->payloadOffset;
                StorylandWorldMesh linked = choice->mesh;
                linked.sectorIndex = sectorIndex;
                worldMeshCache.push_back(std::move(linked));
            } else {
                resolution.source = "conflict";
            }
        } else if (master != masterLvzMeshes.end() && !master->second.empty()) {
            resolution.candidateCount = uint32_t(master->second.size());
            const CandidateMesh* choice = chooseByPlacement(master->second);
            if (choice == nullptr && master->second.size() == 1u) {
                // Master Resource[] is an authoritative ID->payload table.  If it has
                // exactly one valid decoded model for this ID, use it regardless of
                // placement-bound heuristics.
                choice = &master->second.front();
            }
            if (choice != nullptr) {
                resolution.source = "master LVZ";
                resolution.payloadOffset = choice->payloadOffset;
                StorylandWorldMesh linked = choice->mesh;
                linked.sectorIndex = sectorIndex;
                worldMeshCache.push_back(std::move(linked));
            } else {
                resolution.source = "conflict";
            }
        } else if (continuation != continuationMeshes.end() && !continuation->second.empty()) {
            resolution.candidateCount = uint32_t(continuation->second.size());
            const CandidateMesh* choice = chooseByPlacement(continuation->second);
            if (choice == nullptr && continuation->second.size() == 1u) {
                choice = &continuation->second.front();
            }
            if (choice != nullptr) {
                resolution.source = "IMG continuation";
                resolution.payloadOffset = choice->payloadOffset;
                StorylandWorldMesh linked = choice->mesh;
                linked.sectorIndex = sectorIndex;
                worldMeshCache.push_back(std::move(linked));
            } else {
                resolution.source = "conflict";
            }
        } else {
            auto candidates = meshesByResource.find(resourceId);
            resolution.candidateCount = candidates == meshesByResource.end() ? 0u : uint32_t(candidates->second.size());
            if (candidates == meshesByResource.end() || candidates->second.empty()) {
                // A WRLD placement does not imply that its model payload must live
                // in this LVZ/IMG pair. VCS legitimately references models from
                // other streamed archives and the global model pool. Calling those
                // references "missing" made healthy retail data look corrupt.
                bool hasLocalRow = false;
                for (const StorylandImgResourceRow& row : imgResourceRowCache) {
                    if (row.resourceId == resourceId) {
                        hasLocalRow = true;
                        break;
                    }
                }
                resolution.source = hasLocalRow ? "local unsupported" : "external reference";
            } else {
                const CandidateMesh* choice = chooseByPlacement(candidates->second);
                if (choice == nullptr && candidates->second.size() == 1u) {
                    // One structurally valid payload for a used RES is deterministic.
                    // Keep placement-fit scoring for genuine duplicate-ID cases only.
                    choice = &candidates->second.front();
                }
                if (choice != nullptr) {
                    resolution.source = candidates->second.size() == 1u
                        ? "unique linked sector"
                        : "placement-fit exact RES";
                    resolution.payloadOffset = choice->payloadOffset;
                    StorylandWorldMesh linked = choice->mesh;
                    linked.sectorIndex = sectorIndex;
                    worldMeshCache.push_back(std::move(linked));
                } else {
                    resolution.source = "conflict";
                }
            }
        }
        resourceResolutionCache.push_back(std::move(resolution));
    }

    // Keep newly-added master Resource[] models visible even before they have a
    // world placement.  Retail resource availability and placement are separate
    // concepts; a resource does not need a WRLD instance merely to exist.
    for (const auto& namedResource : masterResourceNameOverrides) {
        const uint32_t resourceId = namedResource.first;
        bool alreadyVisible = false;
        for (const StorylandWorldMesh& mesh : worldMeshCache) {
            if (mesh.resourceIndex == resourceId) { alreadyVisible = true; break; }
        }
        if (alreadyVisible) continue;
        auto master = masterLvzMeshes.find(resourceId);
        if (master == masterLvzMeshes.end() || master->second.empty()) continue;
        StorylandWorldMesh unplaced = master->second.front().mesh;
        unplaced.sectorIndex = 0xFFFFFFFFu;
        worldMeshCache.push_back(std::move(unplaced));
    }
}

bool StorylandArchiveBrowser::buildEntriesFromMobileLcsImg(std::string& errorMessage) {
    archiveEntries.clear();
    worldPlacements.clear();
    worldSectors.clear();
    worldMeshCache.clear();
    masterMeshResourceIdCache.clear();
    directTextureCache.clear();
    imgResourceRowCache.clear();
    resourceResolutionCache.clear();

    if (currentImgBytes.size() < 2048u) {
        errorMessage = "IMG is too small to be a Mobile LCS raw gta3.img archive.";
        return false;
    }

    std::map<std::string, uint32_t> duplicateNames;
    uint32_t mdlCount = 0u;
    uint32_t dffCount = 0u;
    uint32_t txdCount = 0u;
    uint32_t textureCount = 0u;

    // May-2005 PSPHR archives contain sector-aligned Leeds MDL payloads and
    // may have no usable companion metadata at all.
    for (size_t mdlSector = 0u; mdlSector * 2048u + 0x20u <= currentImgBytes.size(); ++mdlSector) {
        const size_t offset = mdlSector * 2048u;
        if (readU32(currentImgBytes, offset) != 0x006D646Cu) continue;
        const uint32_t declaredSize = readU32(currentImgBytes, offset + 8u);
        if (declaredSize < 0x20u || declaredSize > 0x08000000u ||
            uint64_t(offset) + uint64_t(declaredSize) > currentImgBytes.size()) continue;

        StorylandArchiveEntry entry;
        entry.index = uint32_t(archiveEntries.size());
        entry.startSector = uint32_t(mdlSector);
        entry.sectorCount = uint32_t((uint64_t(declaredSize) + 2047ull) / 2048ull);
        entry.byteOffset = uint64_t(offset);
        entry.byteSize = declaredSize;
        entry.chunkIdent = MDL_IDENT;
        entry.usesLvzChunkHeader = false;
        char name[96] = {};
        std::snprintf(name, sizeof(name), "beta_psp_mdl_%05u.mdl", entry.startSector);
        entry.name = name;
        archiveEntries.push_back(std::move(entry));
        ++mdlCount;
    }

    size_t sector = 0;
    while (sector * 2048u + 12u <= currentImgBytes.size()) {
        size_t offset = sector * 2048u;
        size_t clumpSize = 0;
        if (!mobileLcsRawClumpAt(currentImgBytes, offset, clumpSize)) {
            ++sector;
            continue;
        }

        StorylandArchiveEntry entry;
        entry.index = uint32_t(archiveEntries.size());
        entry.startSector = uint32_t(sector);
        entry.sectorCount = uint32_t((clumpSize + 2047u) / 2048u);
        entry.byteOffset = uint64_t(offset);
        entry.byteSize = uint64_t(std::min(currentImgBytes.size() - offset,
            std::max<size_t>(2048u, ((clumpSize + 2047u) / 2048u) * 2048u)));
        entry.chunkIdent = 0x10u;
        entry.usesLvzChunkHeader = false;

        std::string stem = mobileLcsFrameNameFromClump(currentImgBytes, offset, clumpSize);
        if (stem.empty()) {
            char fallback[64] = {};
            std::snprintf(fallback, sizeof(fallback), "mobile_lcs_%05u", entry.index);
            stem = fallback;
        }
        std::string key = stem;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        uint32_t duplicate = duplicateNames[key]++;
        if (duplicate != 0u) {
            char suffix[24] = {};
            std::snprintf(suffix, sizeof(suffix), "_%03u", duplicate);
            stem += suffix;
        }
        entry.name = stem + ".dff";
        archiveEntries.push_back(std::move(entry));
        dffCount++;

        sector += std::max<size_t>(1u, (clumpSize + 2047u) / 2048u);
    }

    for (size_t textureSector = 0u; textureSector * 2048u + 12u <= currentImgBytes.size(); ++textureSector) {
        size_t offset = textureSector * 2048u;
        size_t dictionarySize = 0u;
        std::vector<std::string> textureNames;
        if (!mobileLcsRawTextureDictionaryAt(currentImgBytes, offset, dictionarySize, textureNames)) continue;

        StorylandArchiveEntry entry;
        entry.index = uint32_t(archiveEntries.size());
        entry.startSector = uint32_t(textureSector);
        entry.sectorCount = uint32_t(std::max<size_t>(1u, (dictionarySize + 2047u) / 2048u));
        entry.byteOffset = uint64_t(offset);
        entry.byteSize = uint64_t(std::min(currentImgBytes.size() - offset,
            size_t(entry.sectorCount) * 2048u));
        entry.chunkIdent = 0x16u;
        entry.usesLvzChunkHeader = false;
        entry.textureNames = std::move(textureNames);

        std::string firstTexture = entry.textureNames.empty() ? std::string("textures") : entry.textureNames.front();
        firstTexture = mobileLcsSanitizeEntryStem(firstTexture);
        if (firstTexture.empty()) firstTexture = "textures";
        char name[128] = {};
        std::snprintf(name, sizeof(name), "mobile_lcs_txd_%05u_%s.txd", entry.startSector, firstTexture.c_str());
        entry.name = name;

        textureCount += uint32_t(entry.textureNames.size());
        txdCount++;
        archiveEntries.push_back(std::move(entry));

    }

    if (archiveEntries.empty()) {
        errorMessage = "No sector-aligned RenderWare 3.1 DFF clumps or PSP texture dictionaries were found. This is not the supported Mobile LCS raw gta3.img layout.";
        return false;
    }

    std::sort(archiveEntries.begin(), archiveEntries.end(), [](const StorylandArchiveEntry& a, const StorylandArchiveEntry& b) {
        if (a.byteOffset != b.byteOffset) return a.byteOffset < b.byteOffset;
        return a.chunkIdent < b.chunkIdent;
    });
    for (size_t index = 0u; index < archiveEntries.size(); ++index) archiveEntries[index].index = uint32_t(index);

    currentImgKind = StorylandImgKind::MobileLcsRaw;
    currentLevelSummary = "Mobile LCS raw IMG scan: " + std::to_string(mdlCount) +
        " sector-aligned Leeds MDLs, " + std::to_string(dffCount) +
        " RenderWare DFF clumps, " + std::to_string(txdCount) +
        " PSP RenderWare texture dictionaries, " + std::to_string(textureCount) +
        " named textures; DIR/LVZ metadata is optional.";
    currentImgSize = currentImgBytes.size();
    errorMessage.clear();
    return true;
}

bool StorylandArchiveBrowser::buildEntriesFromRawPs2StoriesImg(std::string& errorMessage) {
    archiveEntries.clear();
    worldPlacements.clear();
    worldSectors.clear();
    worldMeshCache.clear();
    masterMeshResourceIdCache.clear();
    directTextureCache.clear();
    imgResourceRowCache.clear();
    resourceResolutionCache.clear();

    if (currentImgBytes.size() < 2048u) {
        errorMessage = "IMG is too small to be a PS2 Stories streaming archive.";
        return false;
    }

    struct RawStart {
        uint32_t sector = 0;
        uint32_t ident = 0;
        const char* ext = nullptr;
        const char* label = nullptr;
    };
    std::vector<RawStart> starts;
    uint32_t xtxCount = 0, mdlCount = 0, animCount = 0, col2Count = 0;
    const size_t sectorCount = currentImgBytes.size() / 2048u;
    starts.reserve(sectorCount / 8u);

    for (size_t sector = 0; sector < sectorCount; ++sector) {
        const size_t off = sector * 2048u;
        const uint32_t ident = readU32(currentImgBytes, off);
        RawStart start;
        start.sector = static_cast<uint32_t>(sector);
        start.ident = ident;
        if (ident == 0x00746578u) { // retail Stories PS2 XTX/XET archive signature
            start.ext = ".xtx"; start.label = "xtx"; ++xtxCount;
        } else if (ident == 0x006d646cu) { // Leeds MDL
            start.ext = ".mdl"; start.label = "mdl"; ++mdlCount;
        } else if (ident == 0x6d696e61u) { // ANIM
            start.ext = ".anim"; start.label = "anim"; ++animCount;
        } else if (ident == 0x636f6c32u) { // COL2
            start.ext = ".col2"; start.label = "col2"; ++col2Count;
        } else {
            continue;
        }
        starts.push_back(start);
    }

    // A raw PS2 Stories IMG has many sector-aligned Leeds MDL/XTX resources.
    // Requiring both families prevents isolated bytes inside some other GTA IMG
    // from being mistaken for a VCS/LCS PS2 streaming archive.
    if (xtxCount < 4u || mdlCount < 4u) {
        errorMessage = "The raw IMG does not contain a convincing PS2 Stories MDL/XTX signature population.";
        return false;
    }

    std::sort(starts.begin(), starts.end(), [](const RawStart& a, const RawStart& b) {
        return a.sector < b.sector;
    });
    for (size_t i = 0; i < starts.size(); ++i) {
        const uint32_t startSector = starts[i].sector;
        const uint32_t nextSector = i + 1u < starts.size()
            ? starts[i + 1u].sector
            : static_cast<uint32_t>(sectorCount);
        if (nextSector <= startSector) continue;

        StorylandArchiveEntry entry;
        entry.index = static_cast<uint32_t>(archiveEntries.size());
        entry.startSector = startSector;
        entry.sectorCount = nextSector - startSector;
        entry.byteOffset = uint64_t(startSector) * 2048ull;
        entry.byteSize = uint64_t(entry.sectorCount) * 2048ull;
        entry.chunkIdent = starts[i].ident;
        entry.usesLvzChunkHeader = false;
        char name[96] = {};
        std::snprintf(name, sizeof(name), "raw_ps2_%s_s%05u%s", starts[i].label, startSector, starts[i].ext);
        entry.name = name;
        archiveEntries.push_back(std::move(entry));
    }

    currentImgKind = StorylandImgKind::Ps2StoriesRaw;
    currentLevelSummary = "PS2 Stories raw IMG signature scan: " + std::to_string(xtxCount) +
        " XTX, " + std::to_string(mdlCount) + " MDL, " + std::to_string(animCount) +
        " ANIM, " + std::to_string(col2Count) +
        " COL2 candidate starts. GAME.DTZ/LVZ is required for authoritative names and allocation boundaries.";
    currentImgSize = currentImgBytes.size();
    errorMessage.clear();
    return true;
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
        if (ident == WRLD_IDENT) std::snprintf(name, sizeof(name), "world%u.wrld", typeIndex);
        else if (isAreaIdent(ident)) std::snprintf(name, sizeof(name), "area%u.area", typeIndex);
        else if (ident == MDL_IDENT) std::snprintf(name, sizeof(name), "model%u.mdl", typeIndex);
        else if (ident == TEX_IDENT) std::snprintf(name, sizeof(name), "texture%u.xtx", typeIndex);
        else std::snprintf(name, sizeof(name), "resource%u%s", typeIndex, extensionForIdent(ident).c_str());
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
            std::snprintf(name, sizeof(name), "area%u.area", areaCount++);
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

    for (size_t i = 0; i < archiveEntries.size(); ++i) {
        archiveEntries[i].index = uint32_t(i);
        auto overrideIt = archiveNameOverrides.find(archiveEntries[i].byteOffset);
        if (overrideIt != archiveNameOverrides.end()) archiveEntries[i].name = overrideIt->second;
    }

    currentImgKind = StorylandImgKind::LvzPair;
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


bool StorylandArchiveBrowser::buildEntriesFromClassicDir(
    const std::vector<uint8_t>& dirBytes,
    std::string& errorMessage
) {
    archiveEntries.clear();
    worldPlacements.clear();
    worldSectors.clear();
    worldMeshCache.clear();
    masterMeshResourceIdCache.clear();
    directTextureCache.clear();
    imgResourceRowCache.clear();
    resourceResolutionCache.clear();

    if (dirBytes.empty() || (dirBytes.size() % 32u) != 0u) {
        errorMessage = "Classic GTA DIR size is not a multiple of 32 bytes.";
        return false;
    }
    if (currentImgBytes.empty()) {
        errorMessage = "Companion IMG is empty.";
        return false;
    }

    const size_t rowCount = dirBytes.size() / 32u;
    if (rowCount == 0u || rowCount > 200000u) {
        errorMessage = "Classic GTA DIR entry count is invalid.";
        return false;
    }

    archiveEntries.reserve(rowCount);
    size_t validRows = 0u;
    size_t mdlRows = 0u;
    size_t rwRows = 0u;

    for (size_t rowIndex = 0u; rowIndex < rowCount; ++rowIndex) {
        const size_t row = rowIndex * 32u;
        const uint32_t startSector = readU32(dirBytes, row + 0u);
        const uint32_t sectorCount = readU32(dirBytes, row + 4u);
        if (sectorCount == 0u) continue;

        const uint64_t byteOffset = uint64_t(startSector) * 2048ull;
        const uint64_t sectorBytes = uint64_t(sectorCount) * 2048ull;
        if (byteOffset > currentImgBytes.size() ||
            sectorBytes > uint64_t(currentImgBytes.size()) - byteOffset) {
            continue;
        }

        std::string name;
        for (size_t c = 0u; c < 24u; ++c) {
            const unsigned char ch = dirBytes[row + 8u + c];
            if (ch == 0u) break;
            if (ch < 32u || ch >= 127u) { name.clear(); break; }
            name.push_back(char(ch));
        }
        if (name.empty()) name = "entry_" + std::to_string(rowIndex) + ".bin";

        StorylandArchiveEntry entry;
        entry.index = uint32_t(archiveEntries.size());
        entry.startSector = startSector;
        entry.sectorCount = sectorCount;
        entry.byteOffset = byteOffset;
        entry.byteSize = sectorBytes;
        entry.usesLvzChunkHeader = false;
        entry.name = name;

        const size_t off = size_t(byteOffset);
        if (sectorBytes >= 12u) {
            const uint32_t magic = readU32(currentImgBytes, off + 0u);
            const uint32_t declared = readU32(currentImgBytes, off + 4u);
            const uint32_t version = readU32(currentImgBytes, off + 8u);

            if (magic == 0x006D646Cu) { // "mdl\0"
                const uint32_t mdlSize = declared;
                if (mdlSize >= 0x20u && mdlSize <= sectorBytes) {
                    entry.byteSize = mdlSize;
                }
                const size_t dot = entry.name.find_last_of('.');
                if (dot != std::string::npos) entry.name.resize(dot);
                entry.name += ".mdl";
                entry.chunkIdent = MDL_IDENT;
                ++mdlRows;
            } else if ((magic == 0x10u || magic == 0x16u) &&
                       (version == 0x00000310u || version == 0x1003FFFFu) &&
                       uint64_t(declared) + 12ull <= sectorBytes) {
                entry.byteSize = uint64_t(declared) + 12ull;
                entry.chunkIdent = magic;
                ++rwRows;
            }
        }

        archiveEntries.push_back(std::move(entry));
        ++validRows;
    }

    if (archiveEntries.empty()) {
        errorMessage = "No valid IMG ranges were found in the companion DIR.";
        return false;
    }

    currentImgKind = StorylandImgKind::ClassicDir;
    currentLevelSummary =
        "Classic GTA IMG+DIR: " + std::to_string(validRows) +
        " valid directory entries, " + std::to_string(mdlRows) +
        " MDL payloads, " + std::to_string(rwRows) +
        " RenderWare payloads. DIR sectors use 2048-byte units.";
    currentImgSize = currentImgBytes.size();
    errorMessage.clear();
    return true;
}

bool StorylandArchiveBrowser::loadDirWithCompanionImg(
    const std::wstring& dirPath,
    std::string& errorMessage
) {
    clear();

    std::vector<uint8_t> dirBytes;
    if (!readWholeFile(dirPath, dirBytes, errorMessage)) return false;

    std::filesystem::path dir(dirPath);
    std::filesystem::path folder = dir.parent_path();
    const std::wstring stem = dir.stem().wstring();
    std::vector<std::filesystem::path> candidates = {
        folder / (stem + L".img"),
        folder / (stem + L".IMG"),
        folder / L"GTA3.IMG",
        folder / L"gta3.img",
        folder / L"GTA3PSPHR.IMG",
        folder / L"gta3psphr.img"
    };

    std::error_code ec;
    std::filesystem::path chosen;
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            chosen = candidate;
            break;
        }
        ec.clear();
    }
    if (chosen.empty()) {
        errorMessage = "Could not find a companion IMG for this beta/classic DIR.";
        return false;
    }

    currentImgPath = chosen.wstring();
    if (!readWholeFile(currentImgPath, currentImgBytes, errorMessage)) {
        clear();
        return false;
    }
    currentImgSize = currentImgBytes.size();

    if (!buildEntriesFromClassicDir(dirBytes, errorMessage)) {
        clear();
        return false;
    }
    return true;
}

bool StorylandArchiveBrowser::loadZmgFromFile(
    const std::wstring& zmgPath,
    std::string& errorMessage
) {
    clear();

    std::vector<uint8_t> packed;
    if (!readWholeFile(zmgPath, packed, errorMessage)) return false;

    std::vector<uint8_t> unpacked;
    if (!inflateLvzBytes(packed, unpacked, errorMessage)) {
        errorMessage = "ZMG is not a valid bounded zlib stream: " + errorMessage;
        return false;
    }

    currentImgPath = zmgPath;
    currentImgBytes = std::move(unpacked);
    currentImgSize = currentImgBytes.size();

    StorylandArchiveEntry entry;
    entry.index = 0u;
    entry.startSector = 0u;
    entry.sectorCount = uint32_t((currentImgBytes.size() + 2047u) / 2048u);
    entry.byteOffset = 0u;
    entry.byteSize = currentImgBytes.size();
    entry.usesLvzChunkHeader = false;

    std::string stem = narrowForMessage(std::filesystem::path(zmgPath).stem().wstring());
    if (stem.empty()) stem = "zmg";

    if (currentImgBytes.size() >= 12u &&
        readU32(currentImgBytes, 0u) == 0x006D646Cu) {
        entry.name = stem + ".mdl";
        entry.chunkIdent = MDL_IDENT;
    } else if (currentImgBytes.size() >= 12u &&
               readU32(currentImgBytes, 0u) == 0x10u &&
               (readU32(currentImgBytes, 8u) == 0x00000310u ||
                readU32(currentImgBytes, 8u) == 0x1003FFFFu)) {
        entry.name = stem + ".dff";
        entry.chunkIdent = 0x10u;
    } else if (currentImgBytes.size() >= 12u &&
               readU32(currentImgBytes, 0u) == 0x16u &&
               (readU32(currentImgBytes, 8u) == 0x00000310u ||
                readU32(currentImgBytes, 8u) == 0x1003FFFFu)) {
        entry.name = stem + ".txd";
        entry.chunkIdent = 0x16u;
    } else {
        entry.name = stem + "_inflated.bin";
        entry.chunkIdent = 0u;
    }

    archiveEntries.push_back(std::move(entry));
    currentLevelSummary =
        "LCS beta ZMG: zlib decompressed " + std::to_string(packed.size()) +
        " bytes to " + std::to_string(currentImgBytes.size()) +
        " bytes. The inflated payload is exposed as an analyzable/exportable resource.";
    errorMessage.clear();
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
    // Classic/beta IMG+DIR pairing must be explicit.  A generic GTA3.DIR in the
    // same folder must never hijack gta3PS2.img/gta3PSP.img and misclassify a
    // retail Stories streaming archive as LCS beta/classic.
    {
        std::filesystem::path img(imgPath);
        std::filesystem::path folder = img.parent_path();
        const std::wstring stemLower = lowerWide(img.stem().wstring());
        std::vector<std::filesystem::path> dirCandidates = {
            folder / (img.stem().wstring() + L".dir"),
            folder / (img.stem().wstring() + L".DIR")
        };
        if (stemLower == L"gta3" || stemLower == L"gta3psphr") {
            dirCandidates.push_back(folder / L"GTA3.DIR");
            dirCandidates.push_back(folder / L"gta3.dir");
        }

        std::error_code ec;
        for (const auto& candidate : dirCandidates) {
            if (!std::filesystem::exists(candidate, ec) || !std::filesystem::is_regular_file(candidate, ec)) {
                ec.clear();
                continue;
            }

            std::vector<uint8_t> dirBytes;
            std::vector<uint8_t> imgBytes;
            std::string probeError;
            if (!readWholeFile(candidate.wstring(), dirBytes, probeError) ||
                !readWholeFile(imgPath, imgBytes, probeError) ||
                dirBytes.empty() || (dirBytes.size() % 32u) != 0u) {
                continue;
            }

            size_t checked = 0u;
            size_t fitting = 0u;
            const size_t rows = std::min<size_t>(dirBytes.size() / 32u, 128u);
            for (size_t i = 0u; i < rows; ++i) {
                const uint32_t startSector = readU32(dirBytes, i * 32u + 0u);
                const uint32_t count = readU32(dirBytes, i * 32u + 4u);
                if (count == 0u) continue;
                ++checked;
                const uint64_t begin = uint64_t(startSector) * 2048ull;
                const uint64_t bytes = uint64_t(count) * 2048ull;
                if (begin <= imgBytes.size() && bytes <= uint64_t(imgBytes.size()) - begin) ++fitting;
            }
            if (checked >= 8u && fitting * 100u >= checked * 95u) {
                clear();
                currentImgPath = imgPath;
                currentImgBytes = std::move(imgBytes);
                currentImgSize = currentImgBytes.size();
                if (buildEntriesFromClassicDir(dirBytes, errorMessage)) return true;
                clear();
            }
        }
    }

    std::wstring lvzPath;
    if (autoFindCompanionLvzForImg(imgPath, lvzPath)) {
        if (!loadLvzWithCompanionImg(lvzPath, errorMessage)) return false;
        std::filesystem::path requested(imgPath);
        std::filesystem::path loaded(currentImgPath);
        if (lowerWide(requested.wstring()) != lowerWide(loaded.wstring())) currentImgPath = imgPath;
        return true;
    }

    clear();
    currentImgPath = imgPath;
    if (!readWholeFile(imgPath, currentImgBytes, errorMessage)) {
        clear();
        return false;
    }
    currentImgSize = currentImgBytes.size();

    // Retail VCS/LCS PS2 streaming IMGs are dominated by sector-aligned Leeds
    // XTX and MDL starts.  Test that before the permissive RenderWare scan so
    // a handful of embedded/chance RW chunk headers cannot label the whole IMG
    // as Mobile LCS.
    std::string ps2Error;
    if (buildEntriesFromRawPs2StoriesImg(ps2Error)) return true;

    // Only after PS2 Stories has been ruled out do we attempt the Mobile LCS
    // RenderWare 3.1 raw-IMG format.
    if (buildEntriesFromMobileLcsImg(errorMessage)) return true;

    const std::string mobileError = errorMessage;
    clear();
    errorMessage = "IMG format is not identified safely. PS2 Stories probe: " + ps2Error +
        " Mobile LCS probe: " + mobileError;
    return false;
}

const std::vector<StorylandArchiveEntry>& StorylandArchiveBrowser::entries() const { return archiveEntries; }
const std::vector<StorylandWorldPlacement>& StorylandArchiveBrowser::placements() const { return worldPlacements; }
const std::vector<StorylandWorldSector>& StorylandArchiveBrowser::sectors() const { return worldSectors; }
const std::vector<StorylandWorldMesh>& StorylandArchiveBrowser::worldMeshes() const { return worldMeshCache; }
const std::vector<uint32_t>& StorylandArchiveBrowser::masterMeshResourceIds() const { return masterMeshResourceIdCache; }
const std::vector<StorylandDirectTextureResource>& StorylandArchiveBrowser::directTextures() const { return directTextureCache; }
const std::vector<StorylandImgResourceRow>& StorylandArchiveBrowser::imgResourceRows() const { return imgResourceRowCache; }
const std::vector<StorylandResourceResolution>& StorylandArchiveBrowser::resourceResolutions() const { return resourceResolutionCache; }
const std::wstring& StorylandArchiveBrowser::imgPath() const { return currentImgPath; }
const std::wstring& StorylandArchiveBrowser::lvzPath() const { return currentLvzPath; }
bool StorylandArchiveBrowser::hasLvzContext() const { return !currentLvzPath.empty(); }
bool StorylandArchiveBrowser::hasImgContext() const { return !currentImgPath.empty(); }
uint64_t StorylandArchiveBrowser::imgFileSize() const { return currentImgSize; }
std::string StorylandArchiveBrowser::levelSummary() const { return currentLevelSummary; }
StorylandImgKind StorylandArchiveBrowser::imgKind() const { return currentImgKind; }
bool StorylandArchiveBrowser::isPs2StoriesRawImg() const { return currentImgKind == StorylandImgKind::Ps2StoriesRaw; }
bool StorylandArchiveBrowser::isMobileLcsRawImg() const { return currentImgKind == StorylandImgKind::MobileLcsRaw; }
bool StorylandArchiveBrowser::hasClassicDirContext() const { return currentImgKind == StorylandImgKind::ClassicDir; }

std::string StorylandArchiveBrowser::resourceDisplayName(uint32_t resourceId) const {
    auto found = masterResourceNameOverrides.find(resourceId);
    if (found != masterResourceNameOverrides.end() && !found->second.empty()) return found->second;
    return "model" + std::to_string(resourceId) + ".mdl";
}

bool StorylandArchiveBrowser::extractEntryBytes(size_t index, std::vector<uint8_t>& outBytes, std::string& errorMessage) const {
    if (index >= archiveEntries.size()) {
        errorMessage = "Archive entry index out of range.";
        return false;
    }

    const StorylandArchiveEntry& entry = archiveEntries[index];
    if (entry.byteOffset > currentImgBytes.size() ||
        entry.byteSize > uint64_t(currentImgBytes.size()) - entry.byteOffset) {
        errorMessage = "Archive entry byte range is outside the loaded IMG/ZMG buffer.";
        return false;
    }

    const size_t begin = size_t(entry.byteOffset);
    const size_t size = size_t(entry.byteSize);
    std::vector<uint8_t> payload(
        currentImgBytes.begin() + begin,
        currentImgBytes.begin() + begin + size);

    if (entry.usesLvzChunkHeader) {
        if (entry.lvzHeaderOffset > currentLvzBytes.size() ||
            currentLvzBytes.size() - entry.lvzHeaderOffset < 0x20u) {
            errorMessage = "LVZ chunk header offset is invalid.";
            return false;
        }
        outBytes.resize(0x20u + payload.size());
        std::copy(
            currentLvzBytes.begin() + entry.lvzHeaderOffset,
            currentLvzBytes.begin() + entry.lvzHeaderOffset + 0x20u,
            outBytes.begin());
        if (!payload.empty()) {
            std::copy(payload.begin(), payload.end(), outBytes.begin() + 0x20u);
        }
        writeU32(outBytes, 8u, uint32_t(outBytes.size()));
        return true;
    }

    outBytes = std::move(payload);
    return true;
}

bool StorylandArchiveBrowser::rebuildParsedCaches(std::string& errorMessage) {
    if (currentLvzBytes.empty()) return buildEntriesFromMobileLcsImg(errorMessage);
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

    const StorylandArchiveEntry target = archiveEntries[index];
    if (target.byteOffset > currentImgBytes.size() ||
        target.byteSize > uint64_t(currentImgBytes.size()) - target.byteOffset) {
        errorMessage = "Target IMG byte range is invalid.";
        return false;
    }

    const bool replacementHasChunkHeader = replacementBytes.size() >= 0x20u;
    const uint32_t replacementIdent = replacementHasChunkHeader ? readU32(replacementBytes, 0x00u) : 0u;
    const bool replacementIdentMatches =
        replacementIdent == target.chunkIdent ||
        (isAreaIdent(replacementIdent) && isAreaIdent(target.chunkIdent));

    std::vector<uint8_t> newImgBytes;
    std::vector<uint8_t> newLvzHeader;

    if (target.usesLvzChunkHeader) {
        if (replacementHasChunkHeader && replacementIdentMatches) {
            newLvzHeader.assign(replacementBytes.begin(), replacementBytes.begin() + 0x20u);
            newImgBytes.assign(replacementBytes.begin() + 0x20u, replacementBytes.end());
        } else {
            if (target.lvzHeaderOffset + 0x20u > currentLvzBytes.size()) {
                errorMessage = "Target LVZ chunk header offset is invalid.";
                return false;
            }
            newLvzHeader.assign(
                currentLvzBytes.begin() + target.lvzHeaderOffset,
                currentLvzBytes.begin() + target.lvzHeaderOffset + 0x20u);
            newImgBytes = replacementBytes;
        }

        if (newLvzHeader.size() != 0x20u) {
            errorMessage = "Could not build replacement LVZ chunk header.";
            return false;
        }

        writeU32(newLvzHeader, 0x00u, target.chunkIdent);
        writeU32(newLvzHeader, 0x08u, uint32_t(newImgBytes.size() + 0x20u));
        writeU32(newLvzHeader, 0x18u, uint32_t(target.byteOffset));
    } else {
        newImgBytes = replacementBytes;
        if (isAreaIdent(target.chunkIdent) && newImgBytes.size() >= 0x20u) {
            writeU32(newImgBytes, 0x00u, target.chunkIdent);
            writeU32(newImgBytes, 0x08u, uint32_t(newImgBytes.size()));
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

    const uint64_t oldStart = target.byteOffset;
    const uint64_t oldSize = target.byteSize;
    const uint64_t oldEnd = oldStart + oldSize;

    // Retail LVZ/IMG contains pointer-backed structures that are not all represented
    // by Storyland's reconstructed entry list. Shifting following IMG data therefore
    // cannot be made safe merely by updating the visible sChunkHeader rows. Keep every
    // existing IMG offset stable and replace only inside the target's proven allocation.
    uint64_t allocationEnd = oldEnd;
    uint64_t nextKnownOffset = uint64_t(currentImgBytes.size());
    for (const StorylandArchiveEntry& entry : archiveEntries) {
        if (entry.byteOffset > oldStart) nextKnownOffset = std::min<uint64_t>(nextKnownOffset, entry.byteOffset);
    }

    // Permit growth only into verified zero padding before the next known resource.
    // This handles sector padding without moving any subsequent data or pointer.
    if (nextKnownOffset > oldEnd) {
        uint64_t zeroPaddingEnd = oldEnd;
        while (zeroPaddingEnd < nextKnownOffset && zeroPaddingEnd < currentImgBytes.size() &&
               currentImgBytes[size_t(zeroPaddingEnd)] == 0u) {
            ++zeroPaddingEnd;
        }
        allocationEnd = zeroPaddingEnd;
    }

    const uint64_t capacity = allocationEnd - oldStart;
    if (newImgBytes.size() > capacity) {
        std::ostringstream message;
        message << "Replacement is larger than the target's proven in-place IMG allocation.\r\n"
                << "Target: " << target.name << "\r\n"
                << "Old payload: " << oldSize << " bytes\r\n"
                << "Verified in-place capacity: " << capacity << " bytes\r\n"
                << "Replacement payload: " << newImgBytes.size() << " bytes\r\n\r\n"
                << "Storyland refused to shift later IMG resources. Retail LVZ/IMG can contain pointer-backed references "
                   "outside the reconstructed entry list, so moving later resources is not considered runtime-safe.";
        errorMessage = message.str();
        return false;
    }

    if (target.usesLvzChunkHeader && target.lvzHeaderOffset + 0x20u > currentLvzBytes.size()) {
        errorMessage = "Target LVZ header is outside the editable LVZ buffer.";
        return false;
    }
    if (!target.usesLvzChunkHeader && target.lvzHeaderOffset + 12u > currentLvzBytes.size()) {
        errorMessage = "Target AREA descriptor is outside the editable LVZ buffer.";
        return false;
    }

    const std::vector<uint8_t> originalLvzBytes = currentLvzBytes;
    const std::vector<uint8_t> originalImgBytes = currentImgBytes;
    auto rollbackReplacement = [&]() {
        currentLvzBytes = originalLvzBytes;
        currentImgBytes = originalImgBytes;
        std::string ignored;
        rebuildParsedCaches(ignored);
    };

    std::copy(newImgBytes.begin(), newImgBytes.end(), currentImgBytes.begin() + size_t(oldStart));
    std::fill(
        currentImgBytes.begin() + size_t(oldStart + newImgBytes.size()),
        currentImgBytes.begin() + size_t(allocationEnd),
        0u);

    if (target.usesLvzChunkHeader) {
        std::copy(newLvzHeader.begin(), newLvzHeader.end(), currentLvzBytes.begin() + target.lvzHeaderOffset);
        writeU32(currentLvzBytes, target.lvzHeaderOffset + 0x08u, uint32_t(newImgBytes.size() + 0x20u));
        writeU32(currentLvzBytes, target.lvzHeaderOffset + 0x18u, uint32_t(oldStart));
    } else {
        // AREA descriptor layout: id, IMG offset, file size, unknown/resource count.
        // Preserve id/unknown and the original IMG offset; only the size changes.
        writeU32(currentLvzBytes, target.lvzHeaderOffset + 0x04u, uint32_t(oldStart));
        writeU32(currentLvzBytes, target.lvzHeaderOffset + 0x08u, uint32_t(newImgBytes.size()));
    }

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        rollbackReplacement();
        errorMessage = "Replacement was rejected and rolled back because the LVZ/IMG index could not be rebuilt: " + rebuildError;
        return false;
    }

    // Verify that the edited record is still present at exactly the same IMG offset,
    // that its size is the requested size, and that no bytes outside the target's
    // proven allocation changed.
    const StorylandArchiveEntry* rebuiltTarget = nullptr;
    for (const StorylandArchiveEntry& entry : archiveEntries) {
        if (entry.byteOffset != oldStart) continue;
        if (entry.chunkIdent != target.chunkIdent && !(isAreaIdent(entry.chunkIdent) && isAreaIdent(target.chunkIdent))) continue;
        rebuiltTarget = &entry;
        break;
    }
    if (!rebuiltTarget || rebuiltTarget->byteSize != newImgBytes.size()) {
        rollbackReplacement();
        errorMessage = "Replacement verification failed: the rebuilt LVZ/IMG index did not preserve the target offset and replacement size.";
        return false;
    }

    if (!std::equal(newImgBytes.begin(), newImgBytes.end(), currentImgBytes.begin() + size_t(oldStart))) {
        rollbackReplacement();
        errorMessage = "Replacement verification failed: IMG bytes do not match the requested replacement payload.";
        return false;
    }

    for (size_t i = 0; i < originalImgBytes.size(); ++i) {
        if (i >= size_t(oldStart) && i < size_t(allocationEnd)) continue;
        if (currentImgBytes[i] != originalImgBytes[i]) {
            rollbackReplacement();
            errorMessage = "Replacement verification failed: bytes outside the target IMG allocation changed.";
            return false;
        }
    }

    report =
        "Runtime-safe in-place LVZ+IMG replacement\r\n"
        "Resource: " + target.name + "\r\n"
        "IMG offset preserved: " + std::to_string(oldStart) + "\r\n"
        "Old payload bytes: " + std::to_string(oldSize) + "\r\n"
        "New payload bytes: " + std::to_string(newImgBytes.size()) + "\r\n"
        "Verified in-place capacity: " + std::to_string(capacity) + "\r\n"
        "Later IMG offsets shifted: no\r\n"
        "IMG file size changed: no\r\n"
        "Bytes outside target allocation changed: no\r\n"
        "Reparse verification: PASS\r\n"
        "Right-click the archive tree to rebuild or overwrite the LVZ + IMG pair.";

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
        if (sourceLooksLikeLeedsChunk(replacementBytes) && sourceChunkIdentOrMdl(replacementBytes) == MDL_IDENT) {
            StorylandModelFile replacementModel;
            std::string modelError;
            if (!replacementModel.loadFromMemory(replacementBytes, L"replacement.mdl", modelError)) {
                errorMessage = "Replacement MDL could not be parsed as a Leeds model: " + modelError;
                return false;
            }
            if (replacementModel.modelKind() != StorylandModelKind::SimpleModel) {
                errorMessage =
                    "LVZ/IMG WRLD resource replacement accepts SimpleModel MDLs only. "
                    "Detected model kind: " + replacementModel.modelKindName() + ". "
                    "Ped, cutscene, and vehicle MDLs carry runtime structures that cannot be converted into an sBuildingGeometry WRLD resource safely.";
                return false;
            }
            if (replacementModel.previewTriangles().empty() || replacementModel.previewPoints().empty()) {
                errorMessage = "Replacement SimpleModel has no parseable render geometry.";
                return false;
            }
        }
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

    const StorylandDmaTlbReport dmaPreflight = storylandValidatePs2DmaTlb(
        replacementPayload,
        "replacement mesh resource " + std::to_string(resourceId)
    );
    if (!dmaPreflight.safe() || dmaPreflight.vifStreams == 0u || dmaPreflight.vifUnpacks == 0u) {
        errorMessage =
            "Replacement failed the PS2 DMA/VIF/GIF/VU-consumption structural preflight and was not applied.\r\n\r\n" +
            dmaPreflight.text() +
            "\r\nWRLD SimpleModel conversion additionally requires at least one bounded VIF stream and one VIF UNPACK so the sector payload has proven vertex data for VU consumption.";
        return false;
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
        // Some retail placements resolve through the master WRLD Resource[]
        // table rather than a sector-local Resource[] row.  A standalone MDL
        // replacement must therefore be converted to the same WRLD geometry
        // payload and the master row redirected, not copied as an ldm\0 file.
        StorylandMasterResourceTableInfo masterInfo;
        size_t masterRowOffset = 0u;
        uint32_t masterPointer = 0u;
        bool masterIsMesh = false;
        if (masterResourceRow(currentLvzBytes, resourceId, masterInfo, masterRowOffset, masterPointer) &&
            masterPointer >= 0x40u && masterPointer < masterInfo.dataEnd) {
            const size_t masterEnd = masterResourcePayloadEnd(currentLvzBytes, masterInfo, masterPointer);
            StorylandWorldMesh existingMasterMesh;
            size_t existingDescriptor = masterPointer;
            masterIsMesh = parseWorldOverlayMeshNear(
                currentLvzBytes, masterPointer, masterEnd, 0xFFFFFFFFu, resourceId,
                existingMasterMesh, existingDescriptor) &&
                !existingMasterMesh.vertices.empty() && !existingMasterMesh.triangles.empty();
        }

        if (!masterIsMesh) {
            errorMessage = "That real mesh resource id was not found in a sector Resource[] table or as a master WRLD model resource.";
            return false;
        }

        const std::vector<uint8_t> originalLvzBytes = currentLvzBytes;
        const auto originalMasterNames = masterResourceNameOverrides;
        auto rollbackMasterReplacement = [&]() {
            currentLvzBytes = originalLvzBytes;
            masterResourceNameOverrides = originalMasterNames;
            std::string ignored;
            rebuildParsedCaches(ignored);
        };

        uint32_t newPayloadOffset = 0u;
        if (!appendPayloadToMasterResourceSlot(currentLvzBytes, resourceId, replacementPayload,
                                               newPayloadOffset, errorMessage)) {
            rollbackMasterReplacement();
            return false;
        }
        if (masterResourceNameOverrides.find(resourceId) == masterResourceNameOverrides.end()) {
            masterResourceNameOverrides[resourceId] = "resource" + std::to_string(resourceId) + ".mdl";
        }

        std::string rebuildError;
        if (!rebuildParsedCaches(rebuildError)) {
            rollbackMasterReplacement();
            errorMessage = "Master WRLD resource replacement was rolled back because the archive could not be rebuilt: " + rebuildError;
            return false;
        }

        StorylandMasterResourceTableInfo verifyInfo;
        size_t verifyRow = 0u;
        uint32_t verifyPointer = 0u;
        if (!masterResourceRow(currentLvzBytes, resourceId, verifyInfo, verifyRow, verifyPointer) ||
            verifyPointer != newPayloadOffset) {
            rollbackMasterReplacement();
            errorMessage = "Master WRLD resource replacement verification failed: Resource[] did not point at the converted payload.";
            return false;
        }

        const size_t verifyEnd = masterResourcePayloadEnd(currentLvzBytes, verifyInfo, verifyPointer);
        StorylandWorldMesh verifyMesh;
        size_t verifyDescriptor = verifyPointer;
        if (!parseWorldOverlayMeshNear(currentLvzBytes, verifyPointer, verifyEnd, 0xFFFFFFFFu,
                                       resourceId, verifyMesh, verifyDescriptor) ||
            verifyMesh.vertices.empty() || verifyMesh.triangles.empty()) {
            rollbackMasterReplacement();
            errorMessage = "Master WRLD resource replacement verification failed: the redirected payload is not valid geometry.";
            return false;
        }

        std::string pairReport;
        std::string pairError;
        if (!validateLvzImgPair(pairReport, pairError)) {
            rollbackMasterReplacement();
            errorMessage = "Master WRLD resource replacement failed Test LVZ/IMG Pair and was rolled back.\r\n\r\n" + pairError;
            return false;
        }

        report =
            "Converted master WRLD model resource replacement\r\n"
            "Resource id preserved: " + std::to_string(resourceId) + "\r\n"
            "Input converted from standalone MDL: " + std::string(replacementWasConvertedFromMdl ? "yes" : "no; source already matched WRLD mesh payload") + "\r\n"
            "New LVZ payload offset: " + std::to_string(newPayloadOffset) + "\r\n"
            "Converted payload bytes: " + std::to_string(replacementPayload.size()) + "\r\n"
            "Vertices after replacement: " + std::to_string(verifyMesh.vertices.size()) + "\r\n"
            "Triangles after replacement: " + std::to_string(verifyMesh.triangles.size()) + "\r\n"
            "Standalone MDL header/relocations/padding copied: no\r\n"
            "Master Resource[] pointer redirected: yes\r\n"
            "IMG bytes changed: no\r\n"
            "Test LVZ/IMG Pair: PASS";
        return true;
    }

    uint64_t totalCapacity = 0;
    uint64_t totalWritten = 0;
    uint32_t replacedCount = 0;
    uint32_t wrapperOffsetsPreserved = 0;
    const std::vector<uint8_t> originalImgBytes = currentImgBytes;
    auto rollbackMeshReplacement = [&]() {
        currentImgBytes = originalImgBytes;
        std::string ignored;
        rebuildParsedCaches(ignored);
    };

    for (const StorylandSectorResourceSpan& span : spans) {
        if (span.sectorImgOffset > currentImgBytes.size() ||
            span.sectorImgOffset + span.sectorByteSize > currentImgBytes.size() ||
            span.rawOffset > currentImgBytes.size() ||
            span.rawEnd > currentImgBytes.size() ||
            span.rawEnd <= span.rawOffset) {
            errorMessage = "A target mesh resource span is invalid.";
            rollbackMeshReplacement();
            return false;
        }

        size_t payloadOffset = 0;
        if (!findRuntimeSafeSectorPayloadOffset(currentImgBytes, span.sectorImgOffset, span.rawOffset, span.rawEnd, resourceId, payloadOffset)) {
            errorMessage =
                "Could not locate the existing runtime-safe sector mesh payload inside the selected resource. "
                "Replacement was not applied because redirecting Resource[] directly to new bytes can cause the game to read material rows as pointers and TLB-miss.";
            rollbackMeshReplacement();
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
            rollbackMeshReplacement();
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
        rollbackMeshReplacement();
        errorMessage = "Mesh resource replacement was rejected and rolled back because the LVZ/IMG index could not be rebuilt: " + rebuildError;
        return false;
    }

    uint32_t parsedAfter = 0;
    uint32_t trianglesAfter = 0;
    for (const StorylandWorldMesh& mesh : worldMeshCache) {
        if (mesh.resourceIndex != resourceId) continue;
        parsedAfter++;
        trianglesAfter += uint32_t(mesh.triangles.size());
    }
    if (parsedAfter == 0u || trianglesAfter == 0u) {
        rollbackMeshReplacement();
        errorMessage = "Replacement reparsed without a visible WRLD mesh. The LVZ/IMG transaction was rolled back.";
        return false;
    }

    std::string pairReport;
    std::string pairError;
    if (!validateLvzImgPair(pairReport, pairError)) {
        rollbackMeshReplacement();
        errorMessage =
            "Replacement failed the full Test LVZ/IMG Pair verification and was rolled back.\r\n\r\n" + pairError;
        return false;
    }

    report =
        "Runtime-safe real sector mesh replacement\r\n"
        "Resource id preserved: " + std::to_string(resourceId) + "\r\n"
        "Input file converted to sector-resource payload: " + std::string(replacementWasConvertedFromMdl ? "yes; SimpleModel verified; exact MDL VIF packets copied when possible; 24-byte sector material rows" : "no; already sector payload") + "\r\n"
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
        "DMA/VIF/GIF/VU-consumption structural preflight: PASS; tags=" + std::to_string(dmaPreflight.dmaTags) +
        " VIF_streams=" + std::to_string(dmaPreflight.vifStreams) +
        " VIF_UNPACK=" + std::to_string(dmaPreflight.vifUnpacks) +
        " GIF_tags=" + std::to_string(dmaPreflight.gifTags) +
        " warnings=" + std::to_string(dmaPreflight.warnings) + "\r\n"
        "This path avoids the TLB bug where the game read the first material row dword as a pointer.\r\n"
        "Full Test LVZ/IMG Pair after replacement: PASS\r\n"
        "Right-click the archive tree to rebuild or overwrite the LVZ + IMG pair.";

    return true;
}




bool StorylandArchiveBrowser::exportDirectTextureAsXtx(
    size_t textureIndex,
    std::vector<uint8_t>& outBytes,
    std::string& errorMessage
) const {
    outBytes.clear();
    if (textureIndex >= directTextureCache.size()) {
        errorMessage = "Texture index is out of range.";
        return false;
    }

    const StorylandDirectTextureResource& texture = directTextureCache[textureIndex];
    if (texture.width <= 0 || texture.height <= 0 || texture.rgba.empty()) {
        errorMessage = "The selected LVZ/IMG texture has no decoded pixel data.";
        return false;
    }

    const uint8_t outputBpp = texture.bpp <= 4 ? 4u : 8u;
    RgbaImage image;
    image.width = texture.width;
    image.height = texture.height;
    image.rgba = texture.rgba;

    LeedsTextureArchive archive;
    std::wstring virtualPath = L"storyland_lvz_texture.xtx";
    if (texture.name.size() >= 4u) {
        std::string lowerName = texture.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (lowerName.size() >= 4u && lowerName.substr(lowerName.size() - 4u) == ".chk") virtualPath = L"storyland_lvz_texture.chk";
    }
    if (!archive.createEmptyPs2(virtualPath, errorMessage)) return false;

    std::string materialName = texture.materialId >= 0
        ? "texture" + std::to_string(texture.materialId)
        : "texture" + std::to_string(texture.index);
    if (!archive.addTexture(materialName, image, outputBpp, errorMessage)) return false;

    std::string validationReport;
    if (!archive.validateStructure(validationReport, errorMessage)) return false;
    outBytes = archive.rawBytes();
    if (outBytes.empty()) {
        errorMessage = "Standalone XTX writer produced an empty file.";
        return false;
    }
    errorMessage.clear();
    return true;
}

bool StorylandArchiveBrowser::replaceDirectTextureFromArchive(
    size_t textureIndex,
    const std::vector<uint8_t>& replacementBytes,
    std::string& report,
    std::string& errorMessage
) {
    report.clear();
    if (textureIndex >= directTextureCache.size()) {
        errorMessage = "Texture index is out of range.";
        return false;
    }
    if (replacementBytes.empty()) {
        errorMessage = "Replacement texture archive is empty.";
        return false;
    }

    const StorylandDirectTextureResource target = directTextureCache[textureIndex];
    if (target.bpp != 4 && target.bpp != 8) {
        errorMessage = "This LVZ/IMG texture is not a 4bpp or 8bpp PS2 indexed texture, so Storyland will not rewrite it in place.";
        return false;
    }
    if (target.storageBytes == 0u) {
        errorMessage = "Storyland could not determine the selected texture's runtime raster allocation.";
        return false;
    }

    std::vector<uint8_t>& destination = target.storedInImg ? currentImgBytes : currentLvzBytes;
    if (uint64_t(target.dataOffset) + uint64_t(target.storageBytes) > destination.size()) {
        errorMessage = "The selected texture raster allocation is outside the loaded LVZ/IMG buffer.";
        return false;
    }
    const bool directRuntimeTexture =
        uint64_t(target.headerOffset) + 16ull <= destination.size() &&
        readU32(destination, target.headerOffset) == 0xCCCCCCCCu;
    if (!directRuntimeTexture && !target.legacyRaw4bpp) {
        errorMessage = "Storyland can preview this texture, but its writable raster layout is not proven yet.";
        return false;
    }

    LeedsTextureArchive sourceArchive;
    if (!sourceArchive.loadFromMemory(replacementBytes, LeedsPlatform::Auto, errorMessage, L"replacement.xtx")) {
        errorMessage = "Replacement CHK/XTX/TEX could not be parsed: " + errorMessage;
        return false;
    }
    if (sourceArchive.textures().empty()) {
        errorMessage = "Replacement texture archive contains no textures.";
        return false;
    }

    RgbaImage image;
    if (!sourceArchive.decodeTexture(0u, image, errorMessage)) {
        errorMessage = "Could not decode the first replacement texture: " + errorMessage;
        return false;
    }
    if (image.width != target.width || image.height != target.height) {
        std::ostringstream message;
        message << "LVZ/IMG texture replacement is in-place and must preserve the retail raster allocation.\r\n"
                << "Target: " << target.width << "x" << target.height << " " << target.bpp << "bpp\r\n"
                << "Replacement: " << image.width << "x" << image.height << "\r\n\r\n"
                << "Resize the replacement to the target dimensions first. Storyland will preserve the target's "
                   "4bpp/8bpp format and runtime pointer layout.";
        errorMessage = message.str();
        return false;
    }

    const bool swizzled = target.legacyRaw4bpp ? true : (((target.rasterFlags >> 24u) & 0xFFu) != 0u);
    std::vector<uint8_t> raster;
    std::vector<uint8_t> palette;
    if (!leedsEncodeCanonicalPs2TextureBlock(image, uint8_t(target.bpp), swizzled, raster, palette, errorMessage)) {
        return false;
    }
    if (raster.size() + palette.size() != target.storageBytes) {
        std::ostringstream message;
        message << "Replacement encoder produced " << (raster.size() + palette.size())
                << " bytes, but the runtime texture allocation is " << target.storageBytes << " bytes.";
        errorMessage = message.str();
        return false;
    }

    const std::vector<uint8_t> originalLvzBytes = currentLvzBytes;
    const std::vector<uint8_t> originalImgBytes = currentImgBytes;
    auto rollback = [&]() {
        currentLvzBytes = originalLvzBytes;
        currentImgBytes = originalImgBytes;
        std::string ignored;
        rebuildParsedCaches(ignored);
    };

    auto writeAt = destination.begin() + target.dataOffset;
    std::copy(raster.begin(), raster.end(), writeAt);
    std::copy(palette.begin(), palette.end(), writeAt + raster.size());

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        rollback();
        errorMessage = "Texture replacement was rolled back because the LVZ/IMG caches could not be rebuilt: " + rebuildError;
        return false;
    }

    const StorylandDirectTextureResource* verified = nullptr;
    for (const StorylandDirectTextureResource& texture : directTextureCache) {
        if (texture.storedInImg != target.storedInImg) continue;
        if (texture.headerOffset != target.headerOffset) continue;
        if (texture.legacyRaw4bpp != target.legacyRaw4bpp) continue;
        verified = &texture;
        break;
    }
    if (verified == nullptr || verified->width != target.width || verified->height != target.height || verified->bpp != target.bpp) {
        rollback();
        errorMessage = "Texture replacement was rolled back because the runtime texture could not be rediscovered after writing it.";
        return false;
    }

    std::string pairReport;
    std::string pairError;
    if (!validateLvzImgPair(pairReport, pairError)) {
        rollback();
        errorMessage = "Texture replacement failed Test LVZ/IMG Pair and was rolled back.\r\n\r\n" + pairError;
        return false;
    }

    std::ostringstream message;
    message << "Replaced LVZ/IMG texture\r\n"
            << "Texture: " << target.name << "\r\n"
            << "Size: " << target.width << "x" << target.height << "\r\n"
            << "BPP preserved: " << target.bpp << "\r\n"
            << (target.legacyRaw4bpp ? "Raster offset: 0x" : "Runtime header offset: 0x")
            << std::hex << std::uppercase << target.headerOffset << std::dec << "\r\n"
            << "Raster allocation preserved: " << target.storageBytes << " bytes\r\n"
            << "Stored in: " << (target.storedInImg ? "IMG" : "LVZ") << "\r\n"
            << "Pointers shifted: no\r\n"
            << "Test LVZ/IMG Pair: PASS";
    report = message.str();
    errorMessage.clear();
    return true;
}

bool StorylandArchiveBrowser::addWorldPlacement(
    uint32_t resourceId,
    float x,
    float y,
    float z,
    std::string& report,
    std::string& errorMessage
) {
    report.clear();
    if (currentLvzBytes.empty() || currentImgBytes.empty() || worldSectors.empty()) {
        errorMessage = "Open a retail LVZ + IMG pair before placing a model resource.";
        return false;
    }
    if (!finiteReasonable(x, 1000000.0f) || !finiteReasonable(y, 1000000.0f) || !finiteReasonable(z, 1000000.0f)) {
        errorMessage = "Placement coordinates are outside the supported world range.";
        return false;
    }
    if (resourceId > 0xFFFEu) {
        errorMessage = "This placement format stores the resource id in 16 bits.";
        return false;
    }

    StorylandWorldMesh resourceMesh;
    bool haveMesh = false;
    for (const StorylandWorldMesh& mesh : worldMeshCache) {
        if (mesh.resourceIndex != resourceId || mesh.vertices.empty() || mesh.triangles.empty()) continue;
        resourceMesh = mesh;
        haveMesh = true;
        break;
    }
    if (!haveMesh) {
        StorylandMasterResourceTableInfo masterInfo;
        size_t masterRowOffset = 0u;
        uint32_t masterPointer = 0u;
        if (masterResourceRow(currentLvzBytes, resourceId, masterInfo, masterRowOffset, masterPointer) &&
            masterPointer >= 0x40u && masterPointer < masterInfo.dataEnd) {
            const size_t payloadEnd = masterResourcePayloadEnd(currentLvzBytes, masterInfo, masterPointer);
            size_t descriptorOffset = masterPointer;
            if (parseWorldOverlayMeshNear(currentLvzBytes, masterPointer, payloadEnd, 0xFFFFFFFFu,
                                          resourceId, resourceMesh, descriptorOffset) &&
                !resourceMesh.vertices.empty() && !resourceMesh.triangles.empty()) {
                haveMesh = true;
            }
        }
    }
    if (!haveMesh) {
        errorMessage = "The selected resource does not decode as a WRLD model resource, so Storyland cannot create an sGeomInstance placement for it.";
        return false;
    }

    float minX = resourceMesh.vertices.front().x, maxX = minX;
    float minY = resourceMesh.vertices.front().y, maxY = minY;
    float minZ = resourceMesh.vertices.front().z, maxZ = minZ;
    for (const StorylandWorldMeshVertex& vertex : resourceMesh.vertices) {
        minX = std::min(minX, vertex.x); maxX = std::max(maxX, vertex.x);
        minY = std::min(minY, vertex.y); maxY = std::max(maxY, vertex.y);
        minZ = std::min(minZ, vertex.z); maxZ = std::max(maxZ, vertex.z);
    }
    const float localCenterX = (minX + maxX) * 0.5f;
    const float localCenterY = (minY + maxY) * 0.5f;
    const float localCenterZ = (minZ + maxZ) * 0.5f;
    float localRadius = 0.5f;
    for (const StorylandWorldMeshVertex& vertex : resourceMesh.vertices) {
        const float dx = vertex.x - localCenterX;
        const float dy = vertex.y - localCenterY;
        const float dz = vertex.z - localCenterZ;
        localRadius = std::max(localRadius, std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    const StorylandWorldSector* selectedSector = nullptr;
    double bestDistance = std::numeric_limits<double>::max();
    for (const StorylandWorldSector& sector : worldSectors) {
        const double dx = double(x) - double(sector.originX);
        const double dy = double(y) - double(sector.originY);
        const double distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            selectedSector = &sector;
        }
    }
    if (selectedSector == nullptr) {
        errorMessage = "Storyland could not choose a WRLD sector for this placement.";
        return false;
    }

    std::vector<std::pair<uint32_t, uint32_t>> sectorRows;
    if (!readSectorRowsFromLvz(currentLvzBytes, sectorRows)) {
        errorMessage = "The WRLD sector row table could not be read.";
        return false;
    }
    const int rowCount = detectWorldGameRowCount(sectorRows.size());
    const int passCount = rowCount == 47 ? 8 : 9;
    int normalPass = -1;
    for (int passIndex = 0; passIndex + 1 < passCount; ++passIndex) {
        if (std::strcmp(passNameForIndex(rowCount, size_t(passIndex)), "NORMAL") == 0) {
            normalPass = passIndex;
            break;
        }
    }
    if (normalPass < 0) {
        errorMessage = "Storyland could not locate the NORMAL placement pass for this WRLD layout.";
        return false;
    }

    const uint64_t sectorStart = selectedSector->imgOffset;
    const uint64_t sectorEnd = selectedSector->imgOffset + selectedSector->byteSize;
    if (sectorStart + 0x08u + uint64_t(passCount) * 4ull > currentImgBytes.size()) {
        errorMessage = "The selected WRLD sector has a truncated pass table.";
        return false;
    }

    std::vector<uint32_t> passPointers(static_cast<size_t>(passCount), 0u);
    for (int passIndex = 0; passIndex < passCount; ++passIndex) {
        passPointers[size_t(passIndex)] = readU32(currentImgBytes, size_t(sectorStart) + 0x08u + size_t(passIndex) * 4u);
    }
    const uint32_t startPointer = passPointers[size_t(normalPass)];
    const uint32_t stopPointer = passPointers[size_t(normalPass + 1)];
    if (startPointer < 0x20u || stopPointer < startPointer) {
        errorMessage = "The NORMAL pass bounds are invalid.";
        return false;
    }
    const uint64_t normalStart64 = sectorStart + uint64_t(startPointer) - 0x20ull;
    const uint64_t normalStop64 = sectorStart + uint64_t(stopPointer) - 0x20ull;
    if (normalStart64 < sectorStart || normalStop64 < normalStart64 || normalStop64 > sectorEnd || normalStop64 > currentImgBytes.size()) {
        errorMessage = "The NORMAL pass range is outside the selected WRLD sector.";
        return false;
    }

    // Placement rows are parsed on a fixed 0x50-byte lattice.  Expanding the
    // pass by one 2048-byte IMG sector is useful for capacity, but 0x800 is not
    // divisible by 0x50.  Appending another placement at the next sector
    // boundary therefore moves it off the parser's row lattice.  Reuse a clean
    // 0x50 slot in the existing NORMAL span first; the first expansion creates
    // many such slots for subsequent instances of the same or other models.
    size_t placementWriteOffset = SIZE_MAX;
    size_t lastRecognizedRow = SIZE_MAX;
    for (uint64_t row = normalStart64; row + 0x50ull <= normalStop64; row += 0x50ull) {
        if (looksLikeImgInstanceRow(currentImgBytes, size_t(row), 0xFFFFu)) lastRecognizedRow = size_t(row);
    }
    size_t candidate = lastRecognizedRow == SIZE_MAX ? size_t(normalStart64) : lastRecognizedRow + 0x50u;
    if (candidate + 0x50u <= size_t(normalStop64)) {
        bool allZero = true;
        for (size_t byteIndex = candidate; byteIndex < candidate + 0x50u; ++byteIndex) {
            if (currentImgBytes[byteIndex] != 0u) { allZero = false; break; }
        }
        if (allZero) placementWriteOffset = candidate;
    }

    constexpr size_t insertionBytes = 2048u;
    const bool growNormalPass = placementWriteOffset == SIZE_MAX;
    const uint64_t insertionOffset64 = normalStop64;
    const size_t insertionOffset = size_t(insertionOffset64);
    if (growNormalPass) placementWriteOffset = insertionOffset;

    std::vector<uint8_t> insertedRow(0x50u, 0u);
    for (const StorylandWorldPlacement& placement : worldPlacements) {
        if (placement.sectorIndex != selectedSector->sectorIndex || int(placement.passIndex) != normalPass) continue;
        if (placement.imgOffset + 0x50ull > currentImgBytes.size()) continue;
        std::copy(currentImgBytes.begin() + size_t(placement.imgOffset),
                  currentImgBytes.begin() + size_t(placement.imgOffset) + 0x50u,
                  insertedRow.begin());
        break;
    }

    std::set<uint32_t> usedIplIds;
    for (const StorylandWorldPlacement& placement : worldPlacements) usedIplIds.insert(placement.iplId & 0x7FFFu);
    uint32_t newIplId = 1u;
    while (newIplId < 0x7FFFu && usedIplIds.find(newIplId) != usedIplIds.end()) ++newIplId;
    if (newIplId >= 0x7FFFu) {
        errorMessage = "No free 15-bit IPL instance id is available.";
        return false;
    }

    writeU16(insertedRow, 0x00u, uint16_t(newIplId));
    writeU16(insertedRow, 0x02u, uint16_t(resourceId));
    writeU16(insertedRow, 0x04u, floatToHalf(x + localCenterX));
    writeU16(insertedRow, 0x06u, floatToHalf(y + localCenterY));
    writeU16(insertedRow, 0x08u, floatToHalf(z + localCenterZ));
    writeU16(insertedRow, 0x0Au, floatToHalf(localRadius));
    writeU32(insertedRow, 0x0Cu, 0u);

    for (int matrixIndex = 0; matrixIndex < 16; ++matrixIndex) writeF32(insertedRow, 0x10u + size_t(matrixIndex) * 4u, 0.0f);
    writeF32(insertedRow, 0x10u + 0u * 4u, 1.0f);
    writeF32(insertedRow, 0x10u + 5u * 4u, 1.0f);
    writeF32(insertedRow, 0x10u + 10u * 4u, 1.0f);
    writeF32(insertedRow, 0x10u + 12u * 4u, x - selectedSector->originX);
    writeF32(insertedRow, 0x10u + 13u * 4u, y - selectedSector->originY);
    writeF32(insertedRow, 0x10u + 14u * 4u, z - selectedSector->originZ);
    writeF32(insertedRow, 0x10u + 15u * 4u, 1.0f);

    const std::vector<uint8_t> originalLvzBytes = currentLvzBytes;
    const std::vector<uint8_t> originalImgBytes = currentImgBytes;
    auto rollback = [&]() {
        currentLvzBytes = originalLvzBytes;
        currentImgBytes = originalImgBytes;
        std::string ignored;
        rebuildParsedCaches(ignored);
    };

    if (growNormalPass) {
        currentImgBytes.insert(currentImgBytes.begin() + insertionOffset, insertionBytes, uint8_t(0));

        // The first new row occupies the old NORMAL end.  All later passes in
        // this sector move by one 2048-byte IMG sector, preserving their sector
        // alignment.  Additional rows are written into the zero capacity this
        // expansion created instead of inserting another misaligned sector.
        for (int passIndex = normalPass + 1; passIndex < passCount; ++passIndex) {
            const size_t field = size_t(sectorStart) + 0x08u + size_t(passIndex) * 4u;
            writeU32(currentImgBytes, field, passPointers[size_t(passIndex)] + uint32_t(insertionBytes));
        }

        if (uint64_t(selectedSector->headerOffset) + 0x20ull > currentLvzBytes.size()) {
            rollback();
            errorMessage = "The selected sector's LVZ header is outside the editable LVZ buffer.";
            return false;
        }
        const uint32_t oldSectorFileSize = readU32(currentLvzBytes, selectedSector->headerOffset + 0x08u);
        if (oldSectorFileSize > uint32_t(-1) - uint32_t(insertionBytes)) {
            rollback();
            errorMessage = "The selected sector size would overflow the Leeds 32-bit field.";
            return false;
        }
        writeU32(currentLvzBytes, selectedSector->headerOffset + 0x08u, oldSectorFileSize + uint32_t(insertionBytes));

        std::set<size_t> patchedImgOffsetFields;
        patchedImgOffsetFields.insert(size_t(selectedSector->headerOffset) + 0x18u);

        for (const StorylandWorldSector& sector : worldSectors) {
            if (sector.sectorIndex == selectedSector->sectorIndex) continue;
            if (sector.imgOffset < insertionOffset64) continue;
            const size_t field = size_t(sector.headerOffset) + 0x18u;
            if (field + 4u > currentLvzBytes.size() || !patchedImgOffsetFields.insert(field).second) continue;
            writeU32(currentLvzBytes, field, readU32(currentLvzBytes, field) + uint32_t(insertionBytes));
        }

        for (const StorylandArchiveEntry& entry : archiveEntries) {
            if (entry.byteOffset < insertionOffset64) continue;
            size_t field = SIZE_MAX;
            if (entry.usesLvzChunkHeader) field = size_t(entry.lvzHeaderOffset) + 0x18u;
            else if (isAreaIdent(entry.chunkIdent)) field = size_t(entry.lvzHeaderOffset) + 0x04u;
            if (field == SIZE_MAX || field + 4u > currentLvzBytes.size() || !patchedImgOffsetFields.insert(field).second) continue;
            writeU32(currentLvzBytes, field, readU32(currentLvzBytes, field) + uint32_t(insertionBytes));
        }
    }

    // Write the actual 0x50-byte instance row after any vector insertion so the
    // destination iterator cannot be invalidated by std::vector::insert.
    if (placementWriteOffset + 0x50u > currentImgBytes.size()) {
        rollback();
        errorMessage = "The placement write slot is outside the rebuilt IMG buffer.";
        return false;
    }
    std::copy(insertedRow.begin(), insertedRow.end(), currentImgBytes.begin() + placementWriteOffset);

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        rollback();
        errorMessage = "Placement insertion was rolled back because the LVZ/IMG index could not be rebuilt: " + rebuildError;
        return false;
    }

    const StorylandWorldPlacement* verified = nullptr;
    for (const StorylandWorldPlacement& placement : worldPlacements) {
        if (placement.resourceIndex != resourceId || placement.iplId != newIplId) continue;
        if (std::fabs(placement.x - x) > 0.25f || std::fabs(placement.y - y) > 0.25f || std::fabs(placement.z - z) > 0.25f) continue;
        verified = &placement;
        break;
    }
    if (verified == nullptr) {
        rollback();
        errorMessage = "The new placement could not be rediscovered after rebuilding the WRLD placement lists, so the edit was rolled back.";
        return false;
    }

    std::string pairReport;
    std::string pairError;
    if (!validateLvzImgPair(pairReport, pairError)) {
        rollback();
        errorMessage = "The new placement failed Test LVZ/IMG Pair and was rolled back.\r\n\r\n" + pairError;
        return false;
    }

    std::ostringstream message;
    message << "Placed model resource\r\n"
            << "Resource: " << resourceDisplayName(resourceId) << "\r\n"
            << "Resource id: " << resourceId << "\r\n"
            << "IPL id: " << newIplId << "\r\n"
            << "Position: " << x << ", " << y << ", " << z << "\r\n"
            << "Sector: " << verified->sectorIndex << " [" << verified->sectorX << "," << verified->sectorY << "]\r\n"
            << "Pass: NORMAL\r\n"
            << "Inserted IMG bytes: " << (growNormalPass ? insertionBytes : 0u)
            << (growNormalPass ? " (NORMAL pass capacity expanded; sector alignment preserved)\r\n"
                               : " (reused free NORMAL-pass placement slot)\r\n")
            << "Later IMG offsets updated: " << (growNormalPass ? "yes" : "not needed") << "\r\n"
            << "Test LVZ/IMG Pair: PASS\r\n\r\n"
            << "Run Place Resource again to create another instance of the same model, then Save LVZ + IMG to rebuild the pair on disk.";
    report = message.str();
    errorMessage.clear();
    return true;
}

bool StorylandArchiveBrowser::validateLvzImgPair(std::string& report, std::string& errorMessage) const {
    report.clear();
    errorMessage.clear();
    if (currentLvzBytes.empty() || currentImgBytes.empty() || currentLvzPath.empty() || currentImgPath.empty()) {
        errorMessage = "Open a retail LVZ+IMG pair before running Test LVZ/IMG Pair.";
        return false;
    }

    StorylandArchiveBrowser reparsed = *this;
    std::string reparseError;
    if (!reparsed.rebuildParsedCaches(reparseError)) {
        errorMessage = "LVZ/IMG reparse failed: " + reparseError;
        return false;
    }

    uint32_t fatal = 0;
    uint32_t warnings = 0;
    uint32_t checkedEntries = 0;
    uint32_t checkedSectors = 0;
    uint32_t checkedRows = 0;
    uint32_t checkedMeshes = 0;
    uint32_t dmaSafeMeshes = 0;
    uint32_t vifBackedMeshes = 0;
    uint32_t placementLinks = 0;
    uint32_t externalPlacementLinks = 0;
    uint32_t localUnsupportedPlacementLinks = 0;
    uint32_t masterRowsChecked = 0;
    uint32_t masterActiveRows = 0;
    uint32_t masterModelRows = uint32_t(reparsed.masterMeshResourceIdCache.size());
    std::ostringstream details;

    StorylandMasterResourceTableInfo masterInfo;
    if (locateMasterResourceTable(reparsed.currentLvzBytes, masterInfo)) {
        std::set<uint32_t> relocationFields;
        const uint32_t relocationOffset = readU32(reparsed.currentLvzBytes, 0x0Cu);
        const uint32_t relocationCount = readU32(reparsed.currentLvzBytes, 0x14u);
        if (relocationOffset <= reparsed.currentLvzBytes.size() &&
            uint64_t(relocationOffset) + uint64_t(relocationCount) * 4ull <= reparsed.currentLvzBytes.size()) {
            for (uint32_t i = 0; i < relocationCount; ++i) {
                relocationFields.insert(readU32(reparsed.currentLvzBytes, size_t(relocationOffset) + size_t(i) * 4u));
            }
        }

        for (uint32_t i = 0; i < masterInfo.count; ++i) {
            ++masterRowsChecked;
            const size_t row = size_t(masterInfo.tableOffset) + size_t(i) * masterInfo.stride;
            const uint32_t pointer = readU32(reparsed.currentLvzBytes, row + 0u);
            const uint32_t unknown = readU32(reparsed.currentLvzBytes, row + 4u);
            const uint32_t resourceId = readU32(reparsed.currentLvzBytes, row + 8u);
            if (pointer == 0u && resourceId == 0xFFFFFFFFu) continue;
            ++masterActiveRows;
            if (resourceId != i) {
                ++fatal;
                details << "FATAL master Resource[] id mismatch: row=" << i << " id=" << resourceId << "\r\n";
            }
            if (pointer < 0x40u || pointer >= masterInfo.dataEnd || (pointer & 3u) != 0u) {
                ++fatal;
                details << "FATAL master Resource[] pointer outside LVZ data: row=" << i << " ptr=" << pointer << "\r\n";
            }
            if (unknown != 0u) {
                ++warnings;
                details << "WARN master Resource[] unknown/flags is nonzero: row=" << i << " value=" << unknown << "\r\n";
            }
            if (relocationFields.find(uint32_t(row)) == relocationFields.end()) {
                ++fatal;
                details << "FATAL master Resource[] pointer field missing from relocation table: row=" << i << " field=" << row << "\r\n";
            }
        }
    } else {
        ++warnings;
        details << "WARN master WRLD Resource[] table was not identified.\r\n";
    }

    uint64_t previousEntryEnd = 0;
    const StorylandArchiveEntry* previousEntry = nullptr;
    std::vector<const StorylandArchiveEntry*> sortedEntries;
    for (const auto& entry : reparsed.archiveEntries) sortedEntries.push_back(&entry);
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const auto* a, const auto* b) {
        if (a->byteOffset != b->byteOffset) return a->byteOffset < b->byteOffset;
        return a->index < b->index;
    });
    for (const auto* entry : sortedEntries) {
        ++checkedEntries;
        if (entry->byteOffset > reparsed.currentImgBytes.size() ||
            entry->byteSize > reparsed.currentImgBytes.size() - entry->byteOffset) {
            ++fatal;
            details << "FATAL archive entry outside IMG: " << entry->name << " offset=" << entry->byteOffset << " size=" << entry->byteSize << "\r\n";
        }
        if (entry->byteOffset < previousEntryEnd) {
            const bool entryIsWorldContainer = entry->chunkIdent == WRLD_IDENT || isAreaIdent(entry->chunkIdent);
            const bool previousIsWorldContainer = previousEntry != nullptr &&
                (previousEntry->chunkIdent == WRLD_IDENT || isAreaIdent(previousEntry->chunkIdent));
            // Retail LVZ metadata contains nested WRLD/AERA headers whose IMG
            // spans deliberately overlap their parent world allocation. This is
            // containment metadata, not two files colliding on disk.
            if (!(entryIsWorldContainer && previousIsWorldContainer)) {
                ++warnings;
                details << "WARN overlapping/reused archive allocation: " << entry->name << " starts=" << entry->byteOffset << " previous_end=" << previousEntryEnd << "\r\n";
            }
        }
        if (entry->byteOffset + entry->byteSize >= previousEntryEnd) {
            previousEntryEnd = entry->byteOffset + entry->byteSize;
            previousEntry = entry;
        }
    }

    std::set<uint32_t> resourceIds;
    // A placement may legally resolve through either a sector Resource[] row or
    // the master WRLD Resource[] table.  The old test counted master-only model
    // resources as "missing placement links" even though Storyland could decode
    // and render them correctly.
    for (uint32_t resourceId : reparsed.masterMeshResourceIdCache) resourceIds.insert(resourceId);
    for (const auto& sector : reparsed.worldSectors) {
        ++checkedSectors;
        if (sector.imgOffset > reparsed.currentImgBytes.size() ||
            sector.byteSize > reparsed.currentImgBytes.size() - sector.imgOffset || sector.byteSize < 8u) {
            ++fatal;
            details << "FATAL WRLD sector outside IMG: sector=" << sector.sectorIndex << " offset=" << sector.imgOffset << " size=" << sector.byteSize << "\r\n";
        }
    }
    for (const auto& row : reparsed.imgResourceRowCache) {
        ++checkedRows;
        resourceIds.insert(row.resourceId);
        if (row.payloadOffset >= reparsed.currentImgBytes.size() ||
            row.payloadSize > reparsed.currentImgBytes.size() - row.payloadOffset) {
            ++fatal;
            details << "FATAL Resource[] payload outside IMG: RES=" << row.resourceId << " sector=" << row.sectorIndex << "\r\n";
        }
    }

    for (const auto& placement : reparsed.worldPlacements) {
        bool finite = std::isfinite(placement.boundX) && std::isfinite(placement.boundY) &&
                      std::isfinite(placement.boundZ) && std::isfinite(placement.boundRadius);
        for (float value : placement.matrix) finite = finite && std::isfinite(value);
        if (!finite || placement.boundRadius < 0.0f) {
            ++fatal;
            details << "FATAL non-finite WRLD placement: RES=" << placement.resourceIndex << " sector=" << placement.sectorIndex << "\r\n";
            continue;
        }
        if (resourceIds.count(placement.resourceIndex)) {
            ++placementLinks;
        } else {
            const StorylandResourceResolution* resolution = nullptr;
            for (const auto& candidate : reparsed.resourceResolutionCache) {
                if (candidate.sectorIndex == placement.sectorIndex && candidate.resourceId == placement.resourceIndex) {
                    resolution = &candidate;
                    break;
                }
            }
            if (resolution != nullptr && resolution->source == "external reference") {
                // Retail VCS WRLD sectors are allowed to reference models owned by
                // another streamed archive/global pool. Absence from this pair is
                // therefore not a broken LVZ->IMG link.
                ++externalPlacementLinks;
            } else {
                ++localUnsupportedPlacementLinks;
                ++warnings;
                details << "WARN local placement resource is present but not decoded as a mesh: RES="
                        << placement.resourceIndex << " sector=" << placement.sectorIndex << "\r\n";
            }
        }
    }

    std::set<std::pair<uint32_t, uint64_t>> seenMeshes;
    for (const auto& mesh : reparsed.worldMeshCache) {
        if (!seenMeshes.insert({mesh.resourceIndex, mesh.rawOffset}).second) continue;
        ++checkedMeshes;
        if (mesh.vertices.empty() || mesh.triangles.empty()) {
            ++fatal;
            details << "FATAL empty parsed WRLD mesh: RES=" << mesh.resourceIndex << "\r\n";
            continue;
        }
        bool indicesOk = true;
        for (const auto& tri : mesh.triangles) {
            if (tri.a >= mesh.vertices.size() || tri.b >= mesh.vertices.size() || tri.c >= mesh.vertices.size()) {
                indicesOk = false;
                break;
            }
        }
        if (!indicesOk) {
            ++fatal;
            details << "FATAL triangle index outside vertex buffer: RES=" << mesh.resourceIndex << "\r\n";
        }

        std::vector<uint8_t> resourceBytes;
        std::string extractError;
        if (reparsed.extractWorldMeshResourceBytes(mesh.resourceIndex, resourceBytes, extractError) && !resourceBytes.empty()) {
            const StorylandDmaTlbReport dma = storylandValidatePs2DmaTlb(resourceBytes, "LVZ/IMG RES " + std::to_string(mesh.resourceIndex));
            if (dma.safe()) ++dmaSafeMeshes;
            else {
                ++fatal;
                details << "FATAL DMA/VIF/GIF range failure for RES=" << mesh.resourceIndex << "\r\n";
            }
            if (dma.vifStreams > 0u && dma.vifUnpacks > 0u) ++vifBackedMeshes;
        }
    }

    report =
        "Test LVZ/IMG Pair\r\n"
        "=================\r\n"
        "LVZ: " + std::filesystem::path(reparsed.currentLvzPath).u8string() + "\r\n"
        "IMG: " + std::filesystem::path(reparsed.currentImgPath).u8string() + "\r\n"
        "LVZ bytes (inflated): " + std::to_string(reparsed.currentLvzBytes.size()) + "\r\n"
        "IMG bytes: " + std::to_string(reparsed.currentImgBytes.size()) + "\r\n"
        "Archive entries checked: " + std::to_string(checkedEntries) + "\r\n"
        "WRLD sectors checked: " + std::to_string(checkedSectors) + "\r\n"
        "Resource[] rows checked: " + std::to_string(checkedRows) + "\r\n"
        "Master Resource[] rows checked: " + std::to_string(masterRowsChecked) + "\r\n"
        "Master Resource[] active rows: " + std::to_string(masterActiveRows) + "\r\n"
        "Master model resources detected: " + std::to_string(masterModelRows) + "\r\n"
        "Parsed mesh variants checked: " + std::to_string(checkedMeshes) + "\r\n"
        "PCSX2-semantics DMA/VIF/GIF/GS-safe resource checks: " + std::to_string(dmaSafeMeshes) + "\r\n"
        "VIF/VU1-consumption-backed resource checks: " + std::to_string(vifBackedMeshes) + "\r\n"
        "Placement->local Resource links: " + std::to_string(placementLinks) + "\r\n"
        "External streamed placement refs: " + std::to_string(externalPlacementLinks) + "\r\n"
        "Local non-mesh/unsupported refs: " + std::to_string(localUnsupportedPlacementLinks) + "\r\n"
        "Warnings: " + std::to_string(warnings) + "\r\n"
        "Fatals: " + std::to_string(fatal) + "\r\n\r\n" + details.str();

    if (fatal != 0u) {
        errorMessage = report + "\r\nRESULT: FAIL - the pair is not safe for replacement testing.";
        return false;
    }
    report += "\r\nRESULT: PASS - structural ranges, WRLD links, geometry, DMA source-chain execution, VIF state/UNPACK writes, GIF packet scheduling, GS register traffic, and VU1 MPG/MSCAL residency dependencies passed the bounded PCSX2-semantics dry-run.";
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
    for (const StorylandWorldMesh& mesh : worldMeshCache) {
        if (mesh.resourceIndex == newResourceId) {
            errorMessage = "The new resource id is already used by another parsed mesh. Choose an unused id to avoid merging unrelated Resource[] rows and placements.";
            return false;
        }
    }

    const std::vector<uint8_t> originalImgBytes = currentImgBytes;
    auto rollbackResourceId = [&]() {
        currentImgBytes = originalImgBytes;
        std::string ignored;
        rebuildParsedCaches(ignored);
    };

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
        rollbackResourceId();
        errorMessage = "No sector Resource[] rows or placement rows used the selected resource id.";
        return false;
    }

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        rollbackResourceId();
        errorMessage = "Resource-id change was rejected and rolled back because the LVZ/IMG index could not be rebuilt: " + rebuildError;
        return false;
    }

    report =
        "Changed real sector mesh resource id\r\n"
        "Old id: " + std::to_string(oldResourceId) + "\r\n"
        "New id: " + std::to_string(newResourceId) + "\r\n"
        "Resource[] table rows changed: " + std::to_string(tableRowsChanged) + "\r\n"
        "sGeomInstance placement rows changed: " + std::to_string(placementRowsChanged) + "\r\n"
        "Right-click the archive tree to rebuild or overwrite the LVZ + IMG pair.";

    return true;
}






bool StorylandArchiveBrowser::addResourceBytes(
    const std::string& resourceName,
    const std::vector<uint8_t>& resourceBytes,
    std::string& report,
    std::string& errorMessage,
    uint32_t* addedResourceId
) {
    report.clear();
    errorMessage.clear();
    if (addedResourceId != nullptr) *addedResourceId = 0xFFFFFFFFu;

    if (currentImgKind != StorylandImgKind::LvzPair || currentLvzBytes.empty() || currentImgBytes.empty()) {
        errorMessage = "Add Resource is available for a loaded retail LVZ+IMG pair.";
        return false;
    }
    if (resourceBytes.size() < 0x20u) {
        errorMessage = "The selected file is too small to contain a Leeds resource.";
        return false;
    }
    if (resourceBytes.size() > 0x08000000u) {
        errorMessage = "The selected resource is too large.";
        return false;
    }

    const uint32_t ident = sourceChunkIdentOrMdl(resourceBytes);
    if (!knownChunkIdent(ident) || ident == WRLD_IDENT || isAreaIdent(ident) || ident == GTAG_IDENT) {
        errorMessage =
            "This file is not a supported loose LVZ/IMG resource.\r\n\r\n"
            "Add Resource accepts standalone Leeds MDL and XTX/CHK/TEX files and converts them into the runtime representation used by the loaded archive.";
        return false;
    }

    std::string cleanName = resourceName;
    if (cleanName.empty()) cleanName = std::string(labelForIdent(ident)) + extensionForIdent(ident);
    for (char& ch : cleanName) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20u || c >= 0x7fu || ch == '/' || ch == '\\' || ch == ':') ch = '_';
    }
    if (cleanName.size() > 95u) cleanName.resize(95u);

    // SimpleModel MDLs are not stored as standalone ldm\0 files inside a WRLD
    // Resource[] table.  Retail LVZ files store an sBuildingGeometry-style
    // material/VIF payload and Resource[] points directly at that payload.  The
    // standalone 0x20-byte MDL header, relocation table and sector padding must
    // therefore be discarded and the model geometry converted first.
    if (ident == MDL_IDENT) {
        StorylandModelFile sourceModel;
        std::string modelError;
        if (!sourceModel.loadFromMemory(resourceBytes, L"added_resource.mdl", modelError)) {
            errorMessage = "The selected MDL could not be parsed as a Leeds model: " + modelError;
            return false;
        }
        if (sourceModel.modelKind() != StorylandModelKind::SimpleModel) {
            errorMessage =
                "Add Resource converts world SimpleModel MDLs into WRLD Resource[] geometry.\r\n"
                "Detected model kind: " + sourceModel.modelKindName() + ".\r\n"
                "PED, cutscene and vehicle MDLs are not world sBuildingGeometry resources.";
            return false;
        }
        if (sourceModel.previewTriangles().empty() || sourceModel.previewPoints().empty()) {
            errorMessage = "The selected SimpleModel has no parseable render geometry.";
            return false;
        }

        StorylandMasterResourceTableInfo masterInfo;
        if (!locateMasterResourceTable(currentLvzBytes, masterInfo)) {
            errorMessage =
                "Storyland could not locate the retail master WRLD Resource[] table. "
                "The model was not inserted because inventing a standalone MDL chunk here is not runtime-correct.";
            return false;
        }

        std::set<uint32_t> globallyUsedResourceIds;
        for (const StorylandWorldPlacement& placement : worldPlacements) globallyUsedResourceIds.insert(placement.resourceIndex);
        for (const StorylandImgResourceRow& row : imgResourceRowCache) globallyUsedResourceIds.insert(row.resourceId);
        for (const StorylandWorldMesh& mesh : worldMeshCache) globallyUsedResourceIds.insert(mesh.resourceIndex);
        for (const StorylandDirectTextureResource& texture : directTextureCache) {
            if (texture.materialId >= 0) globallyUsedResourceIds.insert(uint32_t(texture.materialId));
        }

        // Reserve every resource id that appears in a sector Resource[] table,
        // not only ids currently referenced by a placement.  Unplaced sector
        // resources still occupy the global RES namespace and reusing one would
        // make the new master resource collide with retail data.
        for (const StorylandWorldSector& sector : worldSectors) {
            const uint64_t cont = sector.imgOffset;
            const uint64_t sectorEnd = std::min<uint64_t>(currentImgBytes.size(), sector.imgOffset + sector.byteSize);
            if (cont + 8ull > sectorEnd) continue;
            const uint32_t resourcesPointer = readU32(currentImgBytes, size_t(cont) + 0u);
            const uint32_t resourceCount = readU16(currentImgBytes, size_t(cont) + 4u);
            if (resourceCount == 0u || resourceCount > 4096u) continue;
            const uint64_t listStart = cont + uint64_t(resourcesPointer) - 0x20ull;
            if (listStart < cont || listStart >= sectorEnd) continue;

            if (listStart + uint64_t(resourceCount) * 8ull <= sectorEnd) {
                for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
                    const uint64_t row = listStart + uint64_t(rowIndex) * 8ull;
                    const int32_t id = readI32(currentImgBytes, size_t(row));
                    if (id >= 0 && uint32_t(id) < masterInfo.count) globallyUsedResourceIds.insert(uint32_t(id));
                }
            }
            if (listStart + uint64_t(resourceCount) * 12ull <= sectorEnd) {
                for (uint32_t rowIndex = 0; rowIndex < resourceCount; ++rowIndex) {
                    const uint64_t row = listStart + uint64_t(rowIndex) * 12ull;
                    const uint32_t a = readU32(currentImgBytes, size_t(row) + 0u);
                    const uint32_t b = readU32(currentImgBytes, size_t(row) + 4u);
                    const uint32_t c = readU32(currentImgBytes, size_t(row) + 8u);
                    if (a < masterInfo.count) globallyUsedResourceIds.insert(a);
                    if (b < masterInfo.count) globallyUsedResourceIds.insert(b);
                    if (c < masterInfo.count) globallyUsedResourceIds.insert(c);
                }
            }
        }

        uint32_t resourceId = 0xFFFFFFFFu;
        for (uint32_t i = masterInfo.count; i > 0u; --i) {
            const uint32_t candidate = i - 1u;
            if (candidate <= 1u) continue; // retail keeps the first ids special/empty in several archives
            if (globallyUsedResourceIds.find(candidate) != globallyUsedResourceIds.end()) continue;
            const size_t row = size_t(masterInfo.tableOffset) + size_t(candidate) * masterInfo.stride;
            const uint32_t pointer = readU32(currentLvzBytes, row + 0u);
            const uint32_t id = readU32(currentLvzBytes, row + 8u);
            if (pointer == 0u && id == 0xFFFFFFFFu) {
                resourceId = candidate;
                break;
            }
        }
        if (resourceId == 0xFFFFFFFFu) {
            errorMessage =
                "The master WRLD Resource[] table has no unused retail slot. "
                "Storyland will not enlarge/relocate the table until that layout is proven for this archive.";
            return false;
        }

        // A loose MDL carries texture names rather than WRLD Resource[] texture
        // ids.  Keep the geometry valid and use texture id 0 until a texture
        // resource/binding is explicitly supplied; do not copy file-local MDL
        // material pointers into the WRLD payload.
        std::vector<uint8_t> convertedPayload;
        if (!buildWorldSectorMeshPayloadFromLeedsChunk(resourceBytes, 0u, convertedPayload)) {
            errorMessage =
                "Storyland parsed the SimpleModel but could not convert its Leeds strip data into the retail WRLD Resource[] mesh payload.";
            return false;
        }

        StorylandWorldMesh convertedMesh;
        if (!parseWorldOverlayMesh(convertedPayload, 0u, convertedPayload.size(), 0xFFFFFFFFu, resourceId, convertedMesh) ||
            convertedMesh.vertices.empty() || convertedMesh.triangles.empty()) {
            errorMessage = "The generated WRLD model resource failed its geometry round-trip check.";
            return false;
        }

        const StorylandDmaTlbReport dmaPreflight = storylandValidatePs2DmaTlb(
            convertedPayload,
            "added master WRLD resource " + std::to_string(resourceId)
        );
        if (!dmaPreflight.safe() || dmaPreflight.vifStreams == 0u || dmaPreflight.vifUnpacks == 0u) {
            errorMessage =
                "The converted model failed the PS2 DMA/VIF structural preflight and was not inserted.\r\n\r\n" +
                dmaPreflight.text();
            return false;
        }

        const std::vector<uint8_t> originalLvzBytes = currentLvzBytes;
        const auto originalMasterNames = masterResourceNameOverrides;
        auto rollback = [&]() {
            currentLvzBytes = originalLvzBytes;
            masterResourceNameOverrides = originalMasterNames;
            std::string ignored;
            rebuildParsedCaches(ignored);
        };

        uint32_t payloadOffset = 0u;
        if (!appendPayloadToMasterResourceSlot(currentLvzBytes, resourceId, convertedPayload, payloadOffset, errorMessage)) {
            rollback();
            return false;
        }
        masterResourceNameOverrides[resourceId] = cleanName;

        std::string rebuildError;
        if (!rebuildParsedCaches(rebuildError)) {
            rollback();
            errorMessage = "The converted model was rolled back because the edited LVZ could not be rebuilt: " + rebuildError;
            return false;
        }

        StorylandMasterResourceTableInfo rebuiltInfo;
        size_t rebuiltRow = 0u;
        uint32_t rebuiltPointer = 0u;
        if (!masterResourceRow(currentLvzBytes, resourceId, rebuiltInfo, rebuiltRow, rebuiltPointer) ||
            rebuiltPointer != payloadOffset) {
            rollback();
            errorMessage = "The converted model was rolled back because its Resource[] pointer did not survive the rebuild.";
            return false;
        }
        const size_t payloadEnd = masterResourcePayloadEnd(currentLvzBytes, rebuiltInfo, rebuiltPointer);
        StorylandWorldMesh verificationMesh;
        size_t descriptorOffset = rebuiltPointer;
        if (!parseWorldOverlayMeshNear(currentLvzBytes, rebuiltPointer, payloadEnd, 0xFFFFFFFFu,
                                       resourceId, verificationMesh, descriptorOffset) ||
            verificationMesh.vertices.empty() || verificationMesh.triangles.empty()) {
            rollback();
            errorMessage = "The converted model was rolled back because the new master Resource[] payload no longer parsed as geometry.";
            return false;
        }

        std::string validationReport;
        std::string validationError;
        if (!validateLvzImgPair(validationReport, validationError)) {
            rollback();
            errorMessage = "The converted model failed Test LVZ/IMG Pair and was rolled back.\r\n\r\n" + validationError;
            return false;
        }

        if (addedResourceId != nullptr) *addedResourceId = resourceId;
        std::ostringstream message;
        message << "Added WRLD model resource\r\n"
                << "Name: " << cleanName << "\r\n"
                << "Resource id: " << resourceId << "\r\n"
                << "Master Resource[] row: 0x" << std::hex << std::uppercase << rebuiltRow << std::dec << "\r\n"
                << "LVZ geometry payload: 0x" << std::hex << std::uppercase << payloadOffset << std::dec << "\r\n"
                << "Converted payload bytes: " << convertedPayload.size() << "\r\n"
                << "Vertices: " << verificationMesh.vertices.size() << "\r\n"
                << "Triangles: " << verificationMesh.triangles.size() << "\r\n"
                << "Standalone MDL header copied: no\r\n"
                << "Standalone relocation table copied: no\r\n"
                << "Standalone sector padding copied: no\r\n"
                << "Resource[] relocation entry added: yes\r\n"
                << "IMG bytes changed: no; this is a master WRLD-resident model resource\r\n"
                << "World placement created: no\r\n"
                << "Test LVZ/IMG Pair: PASS\r\n\r\n"
                << "The resource exists independently of a WRLD placement. A placement can reference resource id "
                << resourceId << " later.";
        report = message.str();
        return true;
    }

    // Texture archives still use their LVZ/IMG chunk representation here.  The
    // model path above deliberately does not pass through this code because a
    // SimpleModel Resource[] object is not an ldm\0 chunk at runtime.
    std::string cleanLower = cleanName;
    std::transform(cleanLower.begin(), cleanLower.end(), cleanLower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const StorylandArchiveEntry& entry : archiveEntries) {
        std::string existing = entry.name;
        std::transform(existing.begin(), existing.end(), existing.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (existing == cleanLower) {
            errorMessage = "An archive resource with this name already exists: " + cleanName;
            return false;
        }
    }

    std::vector<uint8_t> newHeader;
    for (const StorylandArchiveEntry& entry : archiveEntries) {
        if (!entry.usesLvzChunkHeader || entry.chunkIdent != ident) continue;
        if (entry.lvzHeaderOffset + 0x20u > currentLvzBytes.size()) continue;
        newHeader.assign(
            currentLvzBytes.begin() + entry.lvzHeaderOffset,
            currentLvzBytes.begin() + entry.lvzHeaderOffset + 0x20u);
        break;
    }
    if (newHeader.empty()) {
        errorMessage =
            std::string("This LVZ does not contain a retail runtime ") + labelForIdent(ident) +
            " chunk header that Storyland can use as a safe template.";
        return false;
    }

    std::vector<uint8_t> newPayload = sourceChunkPayloadForLvzStyle(resourceBytes);
    if (newPayload.empty()) {
        errorMessage = "The selected resource resolves to an empty IMG payload.";
        return false;
    }

    const size_t imgAlignment = 2048u;
    const size_t oldImgSize = currentImgBytes.size();
    const size_t payloadOffset = (oldImgSize + imgAlignment - 1u) & ~(imgAlignment - 1u);
    if (payloadOffset > uint32_t(-1) || newPayload.size() > uint32_t(-1) - payloadOffset) {
        errorMessage = "The resulting IMG offset would exceed the 32-bit Leeds resource address space.";
        return false;
    }

    const size_t oldLvzSize = currentLvzBytes.size();
    const size_t headerOffset = (oldLvzSize + 3u) & ~size_t(3u);

    const std::vector<uint8_t> originalLvzBytes = currentLvzBytes;
    const std::vector<uint8_t> originalImgBytes = currentImgBytes;
    const auto originalNameOverrides = archiveNameOverrides;
    archiveNameOverrides[uint64_t(payloadOffset)] = cleanName;
    auto rollback = [&]() {
        currentLvzBytes = originalLvzBytes;
        currentImgBytes = originalImgBytes;
        archiveNameOverrides = originalNameOverrides;
        std::string ignored;
        rebuildParsedCaches(ignored);
    };

    currentImgBytes.resize(payloadOffset, 0u);
    currentImgBytes.insert(currentImgBytes.end(), newPayload.begin(), newPayload.end());
    const size_t paddedImgEnd = (currentImgBytes.size() + imgAlignment - 1u) & ~(imgAlignment - 1u);
    currentImgBytes.resize(paddedImgEnd, 0u);

    currentLvzBytes.resize(headerOffset, 0u);
    writeU32(newHeader, 0x00u, ident);
    writeU32(newHeader, 0x08u, uint32_t(newPayload.size() + 0x20u));
    writeU32(newHeader, 0x18u, uint32_t(payloadOffset));
    currentLvzBytes.insert(currentLvzBytes.end(), newHeader.begin(), newHeader.end());

    if (currentLvzBytes.size() >= 0x10u) {
        const uint32_t logicalLvzSize = uint32_t(std::min<size_t>(currentLvzBytes.size(), uint32_t(-1)));
        const uint32_t oldTopFileSize = readU32(currentLvzBytes, 0x08u);
        const uint32_t oldTopDataSize = readU32(currentLvzBytes, 0x0Cu);
        if (oldTopFileSize >= 0x20u && oldTopFileSize <= oldLvzSize + 0x1000u)
            writeU32(currentLvzBytes, 0x08u, logicalLvzSize);
        if (oldTopDataSize >= 0x20u && oldTopDataSize <= oldLvzSize + 0x1000u)
            writeU32(currentLvzBytes, 0x0Cu, logicalLvzSize);
    }

    std::string rebuildError;
    if (!rebuildParsedCaches(rebuildError)) {
        rollback();
        errorMessage = "The resource was not added because the rebuilt LVZ/IMG index failed validation: " + rebuildError;
        return false;
    }

    const StorylandArchiveEntry* added = nullptr;
    for (const StorylandArchiveEntry& entry : archiveEntries) {
        if (entry.byteOffset == payloadOffset && entry.chunkIdent == ident) {
            added = &entry;
            break;
        }
    }
    if (!added || added->byteSize != newPayload.size()) {
        rollback();
        errorMessage = "The resource was not added because Storyland could not recover the new LVZ chunk after rebuilding the archive index.";
        return false;
    }

    std::string validationReport;
    std::string validationError;
    if (!validateLvzImgPair(validationReport, validationError)) {
        rollback();
        errorMessage = "The new resource failed LVZ/IMG validation and was rolled back: " + validationError;
        return false;
    }

    std::ostringstream message;
    message << "Added resource: " << cleanName << "\r\n"
            << "Type: " << labelForIdent(ident) << "\r\n"
            << "LVZ header offset: 0x" << std::hex << std::uppercase << headerOffset << std::dec << "\r\n"
            << "IMG offset: " << payloadOffset << "\r\n"
            << "Payload: " << newPayload.size() << " bytes\r\n"
            << "IMG sector: " << (payloadOffset / 2048u) << "\r\n\r\n"
            << "The archive pair is modified in memory. Use Overwrite Current LVZ + IMG or Rebuild LVZ + IMG As to write it.";
    report = message.str();
    currentImgSize = currentImgBytes.size();
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
        std::vector<uint8_t> roundTrip;
        if (!inflateLvzBytes(lvzOut, roundTrip, errorMessage)) {
            errorMessage = "Compressed LVZ round-trip check failed.\r\n" + errorMessage;
            return false;
        }
        if (roundTrip != currentLvzBytes) {
            errorMessage = "Compressed LVZ round-trip check failed: inflated bytes differ from the edited LVZ.";
            return false;
        }
    } else {
        lvzOut = currentLvzBytes;
    }

    if (!storylandWriteFilesTransaction({
            {std::filesystem::path(lvzPath), &lvzOut},
            {std::filesystem::path(imgPath), &currentImgBytes}
        }, errorMessage)) {
        errorMessage = "LVZ+IMG transaction failed; existing pair was restored.\r\n" + errorMessage;
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

bool StorylandArchiveBrowser::findMobileLcsTextureDictionaryForEntry(size_t modelEntryIndex, size_t& outTextureEntryIndex) const {
    if (modelEntryIndex >= archiveEntries.size()) return false;
    const StorylandArchiveEntry& modelEntry = archiveEntries[modelEntryIndex];
    if (modelEntry.chunkIdent != 0x10u || modelEntry.byteOffset >= currentImgBytes.size()) return false;

    size_t available = currentImgBytes.size() - size_t(modelEntry.byteOffset);
    size_t modelSize = size_t(std::min<uint64_t>(modelEntry.byteSize, uint64_t(available)));
    size_t clumpSize = 0u;
    if (mobileLcsRawClumpAt(currentImgBytes, size_t(modelEntry.byteOffset), clumpSize)) {
        modelSize = std::min(modelSize, clumpSize);
    }

    std::vector<std::string> hints = mobileLcsTextureNameHintsFromClump(
        currentImgBytes,
        size_t(modelEntry.byteOffset),
        modelSize
    );
    if (hints.empty()) return false;

    std::string modelStem = modelEntry.name;
    size_t dot = modelStem.find_last_of('.');
    if (dot != std::string::npos) modelStem.resize(dot);
    modelStem = mobileLcsTextureMatchKey(std::move(modelStem));

    int bestScore = 0;
    size_t bestIndex = 0u;
    for (size_t entryIndex = 0u; entryIndex < archiveEntries.size(); ++entryIndex) {
        const StorylandArchiveEntry& candidate = archiveEntries[entryIndex];
        if (candidate.chunkIdent != 0x16u || candidate.textureNames.empty()) continue;

        int score = 0;
        for (const std::string& rawTextureName : candidate.textureNames) {
            std::string textureName = mobileLcsTextureMatchKey(rawTextureName);
            std::string textureLoose = mobileLcsTextureLooseKey(rawTextureName);

            if (!modelStem.empty()) {
                if (textureName == modelStem) score += 50000;
                else if (textureName.find(modelStem) != std::string::npos) score += 12000;
            }

            for (const std::string& rawHint : hints) {
                std::string hint = mobileLcsTextureMatchKey(rawHint);
                std::string hintLoose = mobileLcsTextureLooseKey(rawHint);
                if (hint.empty()) continue;

                if (textureName == hint) score += 100000;
                else if (!hintLoose.empty() && textureLoose == hintLoose) score += 60000;
                else if (hint.size() >= 5u && textureName.find(hint) != std::string::npos) score += 12000;
                else if (textureName.size() >= 5u && hint.find(textureName) != std::string::npos) score += 8000;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestIndex = entryIndex;
        }
    }

    if (bestScore <= 0) return false;
    outTextureEntryIndex = bestIndex;
    return true;
}
