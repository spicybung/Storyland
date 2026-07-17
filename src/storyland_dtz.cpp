#include "storyland_dtz.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <cwctype>
#include <cwchar>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <initializer_list>
#include <utility>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <zlib.h>

static uint32_t readU32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) | (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

static void writeU32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    if (offset + 4 > data.size()) return;
    data[offset + 0] = uint8_t(value & 0xFF);
    data[offset + 1] = uint8_t((value >> 8) & 0xFF);
    data[offset + 2] = uint8_t((value >> 16) & 0xFF);
    data[offset + 3] = uint8_t((value >> 24) & 0xFF);
}

static uint16_t readU16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
}

static int16_t readI16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<int16_t>(readU16(data, offset));
}

static int32_t readI32(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<int32_t>(readU32(data, offset));
}

static float readFloat32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0.0f;
    uint32_t raw = readU32(data, offset);
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

static void writeU16(std::vector<uint8_t>& data, size_t offset, uint16_t value) {
    if (offset + 2 > data.size()) return;
    data[offset + 0] = uint8_t(value & 0xFF);
    data[offset + 1] = uint8_t((value >> 8) & 0xFF);
}

static void writeFloat32(std::vector<uint8_t>& data, size_t offset, float value) {
    uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    writeU32(data, offset, raw);
}

static std::string formatFloat32(float value) {
    std::ostringstream ss;
    ss << std::setprecision(9) << value;
    return ss.str();
}

static std::string readFixedAscii(const std::vector<uint8_t>& data, size_t offset, size_t length) {
    std::string text;
    for (size_t i = 0; i < length && offset + i < data.size(); ++i) {
        char c = char(data[offset + i]);
        if (c == '\0') break;
        if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) {
            text.push_back('.');
        } else {
            text.push_back(c);
        }
    }
    return text;
}

static std::string hex32(uint32_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return ss.str();
}

static std::string hexOffset(uint32_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << value;
    return ss.str();
}

static bool validStreamingPair(uint32_t start, uint32_t count) {
    if (start == 0xFFFFFFFFu || count == 0xFFFFFFFFu) return false;
    if (count == 0 || count > 0x20000u) return false;
    if (start > 0x400000u) return false;
    return true;
}

static bool readWholeFile(const std::wstring& filePath, std::vector<uint8_t>& bytes, std::string& errorMessage) {
    std::ifstream file(std::filesystem::path(filePath), std::ios::binary);
    if (!file) {
        errorMessage = "Could not open file.";
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 0) {
        errorMessage = "Could not determine file size.";
        return false;
    }
    bytes.resize(size_t(size));
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file && size != 0) {
        errorMessage = "Could not read full file.";
        return false;
    }
    return true;
}

static bool writeWholeFileDirect(const std::wstring& filePath, const std::vector<uint8_t>& bytes, std::string& errorMessage) {
    std::ofstream file(std::filesystem::path(filePath), std::ios::binary);
    if (!file) {
        errorMessage = "Could not create output file.";
        return false;
    }
    if (!bytes.empty()) file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    file.flush();
    if (!file) {
        errorMessage = "Could not write complete output file.";
        return false;
    }
    return true;
}

static bool replaceFileWithTempFile(const std::filesystem::path& tempPath, const std::filesystem::path& finalPath, std::string& errorMessage) {
    std::error_code errorCode;
    std::filesystem::remove(finalPath, errorCode);
    errorCode.clear();
    std::filesystem::rename(tempPath, finalPath, errorCode);
    if (errorCode) {
        errorMessage = "Could not replace output file after writing temp file: " + errorCode.message();
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
        return false;
    }
    return true;
}

static bool writeWholeFile(const std::wstring& filePath, const std::vector<uint8_t>& bytes, std::string& errorMessage) {
    if (filePath.empty()) {
        errorMessage = "Output path is empty.";
        return false;
    }

    std::filesystem::path finalPath(filePath);
    std::filesystem::path tempPath = finalPath;
    tempPath += L".storyland_tmp";

    std::error_code cleanupError;
    std::filesystem::remove(tempPath, cleanupError);

    if (!writeWholeFileDirect(tempPath.wstring(), bytes, errorMessage)) {
        std::filesystem::remove(tempPath, cleanupError);
        return false;
    }

    return replaceFileWithTempFile(tempPath, finalPath, errorMessage);
}

static std::wstring asciiWide(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

static std::wstring lowercaseWide(const std::wstring& text) {
    std::wstring result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
    return result;
}

static std::wstring canonicalDtzDirEntryWideName(const std::wstring& displayName) {
    std::wstring lower = lowercaseWide(displayName);
    const wchar_t* knownExtensions[] = {
        L".mdl", L".dff", L".xtx", L".chk", L".tex", L".anim", L".col", L".col2", L".dat", L".ipl", L".ide", L".cut", L".dir", L".bin", L".dtz"
    };

    size_t bestPos = std::wstring::npos;
    size_t bestEnd = std::wstring::npos;
    for (const wchar_t* extension : knownExtensions) {
        size_t pos = lower.find(extension);
        if (pos == std::wstring::npos) continue;
        size_t end = pos + wcslen(extension);
        if (bestPos == std::wstring::npos || pos < bestPos) {
            bestPos = pos;
            bestEnd = end;
        }
    }
    if (bestEnd != std::wstring::npos) return displayName.substr(0, bestEnd);

    size_t noteStart = displayName.find(L" (");
    if (noteStart != std::wstring::npos) return displayName.substr(0, noteStart);
    return displayName;
}

static std::wstring widePathStemLower(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    size_t start = slash == std::wstring::npos ? 0 : slash + 1;
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || dot < start) dot = path.size();
    return lowercaseWide(path.substr(start, dot - start));
}

static std::wstring widePathExtensionLower(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    return lowercaseWide(path.substr(dot));
}


static std::string readDirName(const std::vector<uint8_t>& bytes, size_t offset, size_t length) {
    std::string name;
    for (size_t i = 0; i < length && offset + i < bytes.size(); ++i) {
        char c = char(bytes[offset + i]);
        if (c == '\0') break;
        if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) break;
        name.push_back(c);
    }
    return name;
}

static bool looksLikeDirName(const std::string& name) {
    if (name.empty() || name.size() > 24) return false;
    bool hasDot = false;
    for (char c : name) {
        if (c == '.') hasDot = true;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return hasDot;
}

static bool looksLikeRawDtz(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 4) return false;
    uint32_t magic = readU32(bytes, 0);
    return magic == 0x47544147 || magic == 0x47415447;
}

static bool inflateZlibBytes(const std::vector<uint8_t>& source, std::vector<uint8_t>& unpacked, std::string& errorMessage) {
    if (source.empty()) {
        errorMessage = "Compressed DTZ input is empty.";
        return false;
    }

    z_stream stream = {};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(source.data()));
    stream.avail_in = uInt(source.size());

    int initResult = inflateInit(&stream);
    if (initResult != Z_OK) {
        errorMessage = "zlib inflateInit failed.";
        return false;
    }

    unpacked.clear();
    std::vector<uint8_t> chunk(1024 * 1024);
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = uInt(chunk.size());
        result = inflate(&stream, Z_NO_FLUSH);
        size_t produced = chunk.size() - stream.avail_out;
        unpacked.insert(unpacked.end(), chunk.begin(), chunk.begin() + produced);
    }

    inflateEnd(&stream);

    if (result != Z_STREAM_END) {
        errorMessage = "zlib inflate failed before stream end.";
        return false;
    }
    if (!looksLikeRawDtz(unpacked)) {
        errorMessage = "Inflated file does not look like a raw GAME.DTZ/GATG block.";
        return false;
    }
    return true;
}

static bool deflateZlibBytes(const std::vector<uint8_t>& unpacked, std::vector<uint8_t>& packed, std::string& errorMessage) {
    uLongf bound = compressBound(uLong(unpacked.size()));
    packed.resize(size_t(bound));
    int result = compress2(reinterpret_cast<Bytef*>(packed.data()), &bound, reinterpret_cast<const Bytef*>(unpacked.data()), uLong(unpacked.size()), Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        errorMessage = "zlib compress2 failed.";
        return false;
    }
    packed.resize(size_t(bound));
    return true;
}

static bool validateDeflatedDtzRoundTrip(const std::vector<uint8_t>& expectedUnpacked, const std::vector<uint8_t>& packed, std::string& errorMessage) {
    std::vector<uint8_t> inflated;
    std::string inflateError;
    if (!inflateZlibBytes(packed, inflated, inflateError)) {
        errorMessage = "Compressed DTZ check failed: " + inflateError;
        return false;
    }
    if (inflated.size() != expectedUnpacked.size()) {
        std::ostringstream ss;
        ss << "Compressed DTZ check failed: inflated size "
           << inflated.size()
           << " differs from expected size "
           << expectedUnpacked.size()
           << ".";
        errorMessage = ss.str();
        return false;
    }
    if (!expectedUnpacked.empty() && std::memcmp(inflated.data(), expectedUnpacked.data(), expectedUnpacked.size()) != 0) {
        errorMessage = "Compressed DTZ check failed: inflated bytes differ from the edited raw DTZ bytes.";
        return false;
    }
    return true;
}

static void addHeaderField(std::vector<StorylandDtzHeaderField>& headers, const char* name, uint32_t offset, const std::vector<uint8_t>& data, const char* note) {
    headers.push_back({name, offset, readU32(data, offset), note ? note : ""});
}

static void addHint(std::vector<StorylandDtzResourceHint>& hints, const std::vector<uint8_t>& data, const char* name, uint32_t offsetField, const char* note) {
    uint32_t value = readU32(data, offsetField);
    if (value != 0 && value < data.size()) hints.push_back({name, value, 0, note});
}

static std::string classifyKnownStreamingRecord(uint32_t start, uint32_t count) {
    if (start == 0 && count == 23) return "plr.xtx";
    if (start == 23) return "plr.mdl";
    if (start == 67 && count <= 8) return "first entry after original PLR.mdl end";
    return "";
}

static std::string describeKnownStreamingRecord(uint32_t start, uint32_t count) {
    if (start == 0 && count == 23) return "Known VCS/LCS first texture archive entry: PLR XTX at sectors [0, 23).";
    if (start == 23 && count == 44) return "Known original PLR.mdl mapping: start=23 count=44, byte budget 90112.";
    if (start == 23 && count == 68) return "Known patched PLR.mdl mapping: start=23 count=68, byte budget 139264.";
    if (start == 23) return "PLR.mdl candidate: start sector is the known PLR.mdl base. Count is the editable MDL budget.";
    if (start == 67) return "Entry starts at the original PLR end sector 23+44=67. If PLR is enlarged, this and later starts must shift.";
    return "";
}

bool StorylandDtzArchive::loadFromFile(const std::wstring& filePath, std::string& errorMessage) {
    path = filePath;
    dirPath.clear();
    imgPath.clear();
    dirMap.clear();
    externalDirMap.clear();
    dirRawData.clear();
    imgRawData.clear();

    std::vector<uint8_t> fileBytes;
    if (!readWholeFile(filePath, fileBytes, errorMessage)) return false;

    compressedInput = false;
    if (looksLikeRawDtz(fileBytes)) {
        unpackedData = std::move(fileBytes);
    } else {
        compressedInput = true;
        if (!inflateZlibBytes(fileBytes, unpackedData, errorMessage)) return false;
    }

    if (!parse(errorMessage)) return false;


    tryAutoLoadCompanionImg();
    rebuildDirMatches();
    return true;
}

bool StorylandDtzArchive::loadCompanionDir(const std::wstring& filePath, std::string& errorMessage) {
    std::vector<uint8_t> bytes;
    if (!readWholeFile(filePath, bytes, errorMessage)) return false;
    if (bytes.size() < 32 || (bytes.size() % 32) != 0) {
        errorMessage = "DIR file size is not a multiple of 32 bytes.";
        return false;
    }

    std::vector<StorylandDtzDirEntry> parsed;
    parsed.reserve(bytes.size() / 32);
    for (size_t offset = 0, index = 0; offset + 32 <= bytes.size(); offset += 32, ++index) {
        uint32_t start = readU32(bytes, offset + 0);
        uint32_t count = readU32(bytes, offset + 4);
        std::string name = readDirName(bytes, offset + 8, 24);
        if (!looksLikeDirName(name)) continue;
        if (count == 0 || count > 0x20000 || start > 0x400000) continue;
        StorylandDtzDirEntry entry;
        entry.dirIndex = uint32_t(index);
        entry.startSector = start;
        entry.sectorCount = count;
        entry.name = name;
        parsed.push_back(entry);
    }

    if (parsed.empty()) {
        errorMessage = "No valid IMG/DIR entries were found.";
        return false;
    }

    dirPath = filePath;
    dirRawData = bytes;
    externalDirMap = std::move(parsed);
    rebuildDirMatches();
    return true;
}

bool StorylandDtzArchive::loadCompanionImg(const std::wstring& filePath, std::string& errorMessage) {
    std::vector<uint8_t> bytes;
    if (!readWholeFile(filePath, bytes, errorMessage)) return false;
    if (bytes.empty()) {
        errorMessage = "IMG file is empty.";
        return false;
    }

    imgPath = filePath;
    imgRawData = std::move(bytes);
    rebuildDirMatches();
    return true;
}


void StorylandDtzArchive::tryAutoLoadCompanionDir() {


}

void StorylandDtzArchive::tryAutoLoadCompanionImg() {
    if (path.empty()) return;

    std::error_code ec;
    std::filesystem::path dtzPath(path);
    std::filesystem::path folder = dtzPath.parent_path();
    if (folder.empty()) folder = std::filesystem::current_path(ec);

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(folder / L"gta3PS2.img");
    candidates.push_back(folder / L"GTA3PS2.IMG");
    candidates.push_back(folder / L"gta3ps2.img");
    candidates.push_back(folder / L"gta3PSP.img");
    candidates.push_back(folder / L"GTA3PSP.IMG");
    candidates.push_back(folder / L"gta3psp.img");
    std::filesystem::path sameStemImg = dtzPath;
    sameStemImg.replace_extension(L".img");
    candidates.push_back(sameStemImg);

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) && !std::filesystem::is_directory(candidate, ec)) {
            std::string ignored;
            if (loadCompanionImg(candidate.wstring(), ignored)) return;
        }
    }

    if (!std::filesystem::exists(folder, ec) || !std::filesystem::is_directory(folder, ec)) return;
    for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec)) continue;
        std::filesystem::path candidate = item.path();
        std::wstring ext = candidate.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
        std::wstring stem = candidate.stem().wstring();
        std::transform(stem.begin(), stem.end(), stem.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
        if (ext == L".img" && stem.find(L"gta3") != std::wstring::npos) {
            std::string ignored;
            if (loadCompanionImg(candidate.wstring(), ignored)) return;
        }
    }
}

void StorylandDtzArchive::rebuildDirMatches() {
    for (StorylandDtzSectorRecord& record : records) {
        if (record.resourceName.find(" (same start; count differs") != std::string::npos ||
            record.resourceName.find(" (name by start; count differs") != std::string::npos) {
            record.resourceName.clear();
        }
    }

    dirMap.clear();

    auto recordIsUsable = [](const StorylandDtzSectorRecord& record) -> bool {
        if (record.sectorCount == 0 || record.sectorCount > 0x20000) return false;
        if (record.startSector > 0x400000) return false;
        if (record.startSector == 0xFFFFFFFFu || record.sectorCount == 0xFFFFFFFFu) return false;
        return true;
    };

    auto imgByteAtSector = [&](uint32_t startSector, size_t offsetWithinSector) -> uint8_t {
        uint64_t byteOffset = uint64_t(startSector) * 2048ull + uint64_t(offsetWithinSector);
        if (byteOffset >= imgRawData.size()) return 0;
        return imgRawData[size_t(byteOffset)];
    };

    auto classifySliceExtension = [&](uint32_t startSector, uint32_t sectorCount) -> std::string {
        if (imgRawData.empty()) return "";
        uint64_t byteOffset = uint64_t(startSector) * 2048ull;
        if (byteOffset + 4ull > imgRawData.size()) return "";

        uint8_t b0 = imgByteAtSector(startSector, 0);
        uint8_t b1 = imgByteAtSector(startSector, 1);
        uint8_t b2 = imgByteAtSector(startSector, 2);
        uint8_t b3 = imgByteAtSector(startSector, 3);

        if (b0 == 'l' && b1 == 'd' && b2 == 'm' && b3 == 0) return ".mdl";
        if (b0 == 'x' && b1 == 'e' && b2 == 't' && b3 == 0) return ".xtx";
        if (b0 == 'm' && b1 == 'i' && b2 == 'n' && b3 == 'a') return ".anim";
        if (b0 == 'G' && b1 == 'T' && b2 == 'A' && b3 == 'G') return ".dtz";
        if (b0 == 'G' && b1 == 'A' && b2 == 'T' && b3 == 'G') return ".dtz";
        if (b0 == 'C' && b1 == 'O' && b2 == 'L') return ".col";
        if (b0 == 'I' && b1 == 'D' && b2 == 'E') return ".ide";
        if (b0 == 'I' && b1 == 'P' && b2 == 'L') return ".ipl";


        if (sectorCount <= 2) return ".bin";
        return ".bin";
    };

    auto exactExternalNameForPair = [&](uint32_t startSector, uint32_t sectorCount) -> std::string {


        for (const StorylandDtzDirEntry& externalEntry : externalDirMap) {
            if (externalEntry.startSector == startSector && externalEntry.sectorCount == sectorCount) {
                return externalEntry.name;
            }
        }
        return "";
    };

    auto makeRecoveredName = [&](size_t recoveredIndex, uint32_t startSector, uint32_t sectorCount, const std::string& extension) -> std::string {
        std::ostringstream name;


        std::string prefix = "resource_";
        if (extension == ".anim") prefix = "anim_clip_";
        else if (extension == ".xtx" || extension == ".chk" || extension == ".tex") prefix = "texture_";
        else if (extension == ".mdl" || extension == ".dff") prefix = "model_";
        else if (extension == ".cam") prefix = "camera_";
        else if (extension == ".cut") prefix = "cutscene_";
        else if (extension == ".col" || extension == ".col2") prefix = "collision_";

        name << prefix << std::setw(4) << std::setfill('0') << recoveredIndex
             << "_s" << std::setw(6) << std::setfill('0') << startSector
             << "_c" << std::setw(4) << std::setfill('0') << sectorCount
             << (extension.empty() ? ".bin" : extension);
        return name.str();
    };

    auto lowerAscii = [](std::string text) -> std::string {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return text;
    };

    auto knownNameExtension = [&](const std::string& name) -> std::string {
        std::string lower = lowerAscii(name);
        static const char* extensions[] = {
            ".mdl", ".dff", ".xtx", ".chk", ".tex", ".anim", ".col", ".col2", ".dat", ".ipl", ".ide", ".cut", ".cam", ".bin", ".dtz"
        };
        for (const char* extension : extensions) {
            size_t length = std::strlen(extension);
            if (lower.size() >= length && lower.rfind(extension) == lower.size() - length) return extension;
        }
        return "";
    };

    auto forceNameExtensionFromImgMagic = [&](const std::string& name, const std::string& extension) -> std::string {
        if (extension.empty()) return name;
        std::string oldExtension = knownNameExtension(name);
        if (oldExtension.empty()) return name + extension;
        return name.substr(0, name.size() - oldExtension.size()) + extension;
    };

    auto readCcPaddedAnimName = [&](size_t offset, size_t length, std::string& nameOut) -> bool {
        nameOut.clear();
        if (offset + length > unpackedData.size()) return false;

        for (size_t index = 0; index < length; ++index) {
            uint8_t value = unpackedData[offset + index];
            if (value == 0 || value == 0xCCu) break;
            bool allowed = (value >= 'A' && value <= 'Z') ||
                           (value >= 'a' && value <= 'z') ||
                           (value >= '0' && value <= '9') ||
                           value == '_';
            if (!allowed) return false;
            nameOut.push_back(char(value));
        }

        if (nameOut.size() < 2 || nameOut.size() >= length) return false;
        return true;
    };

    auto canonicalAnimLoaderBaseName = [](std::string name) -> std::string {
        bool hasUnderscore = name.find('_') != std::string::npos;
        bool startsUpper = !name.empty() && name[0] >= 'A' && name[0] <= 'Z';
        if (hasUnderscore && startsUpper && name.size() <= 7u) {
            name.erase(std::remove(name.begin(), name.end(), '_'), name.end());
        }
        return name;
    };

    auto findAnimLoaderArchiveNames = [&]() -> std::vector<std::string> {
        std::vector<std::string> result;
        uint32_t blockOffset = 0;
        if (unpackedData.size() > 0x84u) blockOffset = readU32(unpackedData, 0x80);
        if (blockOffset == 0 || blockOffset >= unpackedData.size()) return result;


        for (size_t row = 0; row < 512u; ++row) {
            size_t rowOffset = size_t(blockOffset) + row * 52u;
            if (rowOffset + 52u > unpackedData.size()) break;

            std::string archiveName;
            std::string hierarchyName;
            if (!readCcPaddedAnimName(rowOffset + 0u, 24u, archiveName)) break;
            if (!readCcPaddedAnimName(rowOffset + 24u, 24u, hierarchyName)) break;

            result.push_back(canonicalAnimLoaderBaseName(archiveName));
        }
        return result;
    };

    auto parseTwentyEightByteNameRow = [&](size_t offset, std::string& nameOut) -> bool {
        nameOut.clear();
        if (offset + 28u > unpackedData.size()) return false;

        size_t cursor = offset;
        while (cursor < offset + 20u) {
            uint8_t value = unpackedData[cursor];
            if (value == 0) break;
            bool allowed = (value >= 'A' && value <= 'Z') ||
                           (value >= 'a' && value <= 'z') ||
                           (value >= '0' && value <= '9') ||
                           value == '_';
            if (!allowed) return false;
            nameOut.push_back(char(value));
            cursor++;
        }

        if (nameOut.size() < 2 || nameOut.size() >= 20u) return false;
        if (unpackedData[offset + nameOut.size()] != 0) return false;

        for (size_t pad = offset + nameOut.size() + 1u; pad < offset + 20u; ++pad) {
            uint8_t value = unpackedData[pad];
            if (value != 0xAAu && value != 0x00u) return false;
        }
        return true;
    };

    auto findInternalStreamingBaseNames = [&]() -> std::vector<std::string> {
        std::vector<std::string> result;
        for (size_t offset = 0; offset + 28u * 8u <= unpackedData.size(); offset += 4u) {
            std::string n0, n1, n2, n3, n4;
            if (!parseTwentyEightByteNameRow(offset + 0u * 28u, n0)) continue;
            if (!parseTwentyEightByteNameRow(offset + 1u * 28u, n1)) continue;
            if (!parseTwentyEightByteNameRow(offset + 2u * 28u, n2)) continue;
            if (!parseTwentyEightByteNameRow(offset + 3u * 28u, n3)) continue;
            if (!parseTwentyEightByteNameRow(offset + 4u * 28u, n4)) continue;


            if (n0 != "plr" || n1 != "cop" || n2 != "swat" || n3 != "fbi" || n4 != "army") {
                continue;
            }

            for (size_t row = 0; row < 4096u && offset + row * 28u + 28u <= unpackedData.size(); ++row) {
                std::string name;
                if (!parseTwentyEightByteNameRow(offset + row * 28u, name)) break;
                result.push_back(name);
            }
            break;
        }
        return result;
    };

    auto sentinelEntryPrefixIsValid = [&](size_t offset) -> bool {
        if (offset + 24u > unpackedData.size()) return false;
        return readU32(unpackedData, offset + 0x00) == 0xAAAAAAAAu &&
               readU32(unpackedData, offset + 0x04) == 0x00000000u &&
               readU32(unpackedData, offset + 0x08) == 0x00000000u &&
               readU32(unpackedData, offset + 0x0C) == 0xAAAA0000u;
    };

    auto findSentinelTableBaseByFirstPair = [&](uint32_t wantedStart, uint32_t wantedCount) -> size_t {
        for (size_t offset = 0; offset + 24u <= unpackedData.size(); offset += 4u) {
            if (!sentinelEntryPrefixIsValid(offset)) continue;
            if (readU32(unpackedData, offset + 0x10) == wantedStart && readU32(unpackedData, offset + 0x14) == wantedCount) {
                return offset;
            }
        }
        return std::numeric_limits<size_t>::max();
    };

    std::map<std::pair<uint32_t, uint32_t>, std::string> internalNameByPair;
    std::vector<std::string> internalBaseNames = findInternalStreamingBaseNames();

    size_t firstStreamingTableBase = findSentinelTableBaseByFirstPair(23u, 44u);
    if (firstStreamingTableBase == std::numeric_limits<size_t>::max()) {
        firstStreamingTableBase = findSentinelTableBaseByFirstPair(23u, 45u);
    }
    if (firstStreamingTableBase == std::numeric_limits<size_t>::max()) {
        firstStreamingTableBase = findSentinelTableBaseByFirstPair(23u, 68u);
    }
    uint32_t streamingInfoHeader = unpackedData.size() > 0x80u ? readU32(unpackedData, 0x7C) : 0u;
    uint32_t modelSlotLimit = 0u;
    uint32_t textureSlotLimit = 0u;
    uint32_t totalSlotLimit = 0u;
    if (streamingInfoHeader != 0u && size_t(streamingInfoHeader) + 0x18u <= unpackedData.size()) {
        modelSlotLimit = readU32(unpackedData, size_t(streamingInfoHeader) + 0x08u);
        textureSlotLimit = readU32(unpackedData, size_t(streamingInfoHeader) + 0x0Cu);
        totalSlotLimit = readU32(unpackedData, size_t(streamingInfoHeader) + 0x14u);
        if (modelSlotLimit == 0u || textureSlotLimit < modelSlotLimit ||
            totalSlotLimit < textureSlotLimit || totalSlotLimit > 0x20000u) {
            modelSlotLimit = 0u;
            textureSlotLimit = 0u;
            totalSlotLimit = 0u;
        }
    }

    auto tableEndFromGlobalSlotLimit = [&](uint32_t slotLimit) -> size_t {
        if (firstStreamingTableBase == std::numeric_limits<size_t>::max() || slotLimit == 0u) {
            return std::numeric_limits<size_t>::max();
        }
        uint64_t end = uint64_t(firstStreamingTableBase) + uint64_t(slotLimit) * 24ull;
        if (end > unpackedData.size()) return std::numeric_limits<size_t>::max();
        return size_t(end);
    };

    auto addNamesFromSentinelTable = [&](size_t tableBase, size_t tableEnd, const char* extension) {
        if (tableBase == std::numeric_limits<size_t>::max()) return;
        if (internalBaseNames.empty()) return;
        if (tableEnd == std::numeric_limits<size_t>::max() || tableEnd <= tableBase) {
            tableEnd = unpackedData.size();
        }

        size_t nameIndex = 0;
        std::string expectedExtension = extension ? extension : "";
        for (size_t row = 0; nameIndex < internalBaseNames.size(); ++row) {
            size_t offset = tableBase + row * 24u;
            if (offset + 24u > tableEnd) break;
            if (!sentinelEntryPrefixIsValid(offset)) break;

            uint32_t start = readU32(unpackedData, offset + 0x10);
            uint32_t count = readU32(unpackedData, offset + 0x14);
            if (!validStreamingPair(start, count)) {
                continue;
            }


            std::string name = internalBaseNames[nameIndex] + expectedExtension;
            internalNameByPair[{start, count}] = name;
            nameIndex++;
        }
    };

    size_t modelTableEnd = tableEndFromGlobalSlotLimit(modelSlotLimit);
    size_t textureTableEnd = tableEndFromGlobalSlotLimit(textureSlotLimit);
    addNamesFromSentinelTable(findSentinelTableBaseByFirstPair(0u, 23u), textureTableEnd, ".xtx");
    addNamesFromSentinelTable(findSentinelTableBaseByFirstPair(23u, 44u), modelTableEnd, ".mdl");
    addNamesFromSentinelTable(findSentinelTableBaseByFirstPair(23u, 45u), modelTableEnd, ".mdl");
    addNamesFromSentinelTable(findSentinelTableBaseByFirstPair(23u, 68u), modelTableEnd, ".mdl");

    auto internalNameForPair = [&](uint32_t startSector, uint32_t sectorCount) -> std::string {
        auto found = internalNameByPair.find({startSector, sectorCount});
        if (found == internalNameByPair.end()) return "";
        return found->second;
    };

    std::vector<std::string> animLoaderArchiveNames = findAnimLoaderArchiveNames();
    size_t unnamedAnimNameIndex = 0;

    auto nextAnimLoaderArchiveName = [&]() -> std::string {
        if (unnamedAnimNameIndex >= animLoaderArchiveNames.size()) return "";
        std::string name = animLoaderArchiveNames[unnamedAnimNameIndex];
        unnamedAnimNameIndex++;
        if (name.empty()) return "";
        return name + ".anim";
    };

    std::set<std::pair<uint32_t, uint32_t>> seenPairs;

    for (size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
        StorylandDtzSectorRecord& record = records[recordIndex];
        if (!recordIsUsable(record)) continue;

        std::pair<uint32_t, uint32_t> key{record.startSector, record.sectorCount};
        if (!seenPairs.insert(key).second) {
            for (StorylandDtzDirEntry& existing : dirMap) {
                if (existing.startSector == record.startSector && existing.sectorCount == record.sectorCount) {
                    existing.matchingRecordIndices.push_back(recordIndex);
                    break;
                }
            }
            continue;
        }

        StorylandDtzDirEntry entry;
        entry.dirIndex = uint32_t(dirMap.size());
        entry.startSector = record.startSector;
        entry.sectorCount = record.sectorCount;
        entry.matchedDtzSectorCount = record.sectorCount;
        entry.countDiffersFromCompanionDir = false;
        entry.matchingRecordIndices.push_back(recordIndex);

        std::string exactExternalName = exactExternalNameForPair(record.startSector, record.sectorCount);
        std::string internalDtzName = internalNameForPair(record.startSector, record.sectorCount);
        std::string extension = classifySliceExtension(record.startSector, record.sectorCount);

        bool recordNameIsUseful = !record.resourceName.empty() && record.resourceName.find("first entry after original") == std::string::npos;
        if (recordNameIsUseful && (record.resourceName == "plr.xtx" || record.resourceName == "plr.mdl")) {
            entry.name = record.resourceName;
        } else if (!internalDtzName.empty()) {
            entry.name = internalDtzName;
        } else if (recordNameIsUseful) {
            entry.name = record.resourceName;
        } else if (!exactExternalName.empty()) {
            entry.name = exactExternalName;
        } else if (extension == ".anim") {
            std::string animLoaderName = nextAnimLoaderArchiveName();
            if (!animLoaderName.empty()) {
                entry.name = animLoaderName;
            } else {
                entry.name = makeRecoveredName(dirMap.size(), record.startSector, record.sectorCount, extension);
                record.note = "ANIM stream slice. GAME.DTZ has the sector range, but no name was found for this clip.";
            }
        } else {
            entry.name = makeRecoveredName(dirMap.size(), record.startSector, record.sectorCount, extension);
            if (entry.name.rfind("resource_", 0) == 0) {
                record.note = "GAME.DTZ stream slice with no recovered name.";
            }
        }


        if (extension == ".mdl" || extension == ".xtx" || extension == ".anim" || extension == ".dtz" || extension == ".col" || extension == ".ide" || extension == ".ipl") {
            entry.name = forceNameExtensionFromImgMagic(entry.name, extension);
        }

        record.resourceName = entry.name;
        dirMap.push_back(entry);
    }

    std::sort(dirMap.begin(), dirMap.end(), [](const StorylandDtzDirEntry& left, const StorylandDtzDirEntry& right) {
        if (left.startSector != right.startSector) return left.startSector < right.startSector;
        if (left.sectorCount != right.sectorCount) return left.sectorCount < right.sectorCount;
        return left.name < right.name;
    });

    for (size_t i = 0; i < dirMap.size(); ++i) {
        dirMap[i].dirIndex = uint32_t(i);
    }
}


bool StorylandDtzArchive::writePatchedCompanionDir(const std::wstring& savedDtzPath, std::string& errorMessage) const {
    if (externalDirMap.empty() || dirRawData.empty()) return true;

    std::vector<uint8_t> patchedDir = dirRawData;
    for (const StorylandDtzDirEntry& entry : externalDirMap) {
        size_t offset = size_t(entry.dirIndex) * 32u;
        if (offset + 8 > patchedDir.size()) continue;
        writeU32(patchedDir, offset + 0, entry.startSector);
        writeU32(patchedDir, offset + 4, entry.sectorCount);
    }

    if (!dirPath.empty()) {
        if (!writeWholeFile(dirPath, patchedDir, errorMessage)) return false;
    }

    if (!savedDtzPath.empty() && !dirPath.empty()) {
        std::filesystem::path saved(savedDtzPath);
        std::filesystem::path originalDir(dirPath);
        std::filesystem::path sibling = saved.parent_path() / originalDir.filename();
        if (!sibling.empty() && sibling.wstring() != dirPath) {
            if (!writeWholeFile(sibling.wstring(), patchedDir, errorMessage)) return false;
        }
    }

    return true;
}

bool StorylandDtzArchive::saveCompanionImgToFile(const std::wstring& filePath, std::string& errorMessage) const {
    if (imgRawData.empty()) {
        errorMessage = "No companion IMG is loaded.";
        return false;
    }
    return writeWholeFile(filePath, imgRawData, errorMessage);
}

bool StorylandDtzArchive::writePatchedCompanionImg(const std::wstring& savedDtzPath, std::string& errorMessage) const {
    if (imgRawData.empty() || imgPath.empty()) return true;
    if (savedDtzPath.empty()) return true;

    std::filesystem::path saved(savedDtzPath);
    std::filesystem::path originalImg(imgPath);
    std::filesystem::path outputDirectory = saved.parent_path();
    std::filesystem::path sibling = outputDirectory / originalImg.filename();

    if (sibling.empty()) {
        errorMessage = "Could not derive companion IMG output path beside rebuilt GAME.DTZ.";
        return false;
    }

    return writeWholeFile(sibling.wstring(), imgRawData, errorMessage);
}

void StorylandDtzArchive::patchCompanionDirMap(
    uint32_t selectedStartSector,
    uint32_t selectedOldSectorCount,
    uint32_t selectedNewSectorCount,
    bool shiftLaterStarts
) {
    if (externalDirMap.empty()) return;

    int64_t delta = int64_t(selectedNewSectorCount) - int64_t(selectedOldSectorCount);
    uint32_t oldEnd = selectedStartSector + selectedOldSectorCount;

    for (StorylandDtzDirEntry& entry : externalDirMap) {
        if (entry.startSector == selectedStartSector) {
            entry.sectorCount = selectedNewSectorCount;
            continue;
        }

        if (!shiftLaterStarts || delta == 0) continue;
        if (entry.startSector < oldEnd) continue;

        int64_t shifted = int64_t(entry.startSector) + delta;
        if (shifted >= 0 && shifted <= 0xFFFFFFFFLL) {
            entry.startSector = uint32_t(shifted);
        }
    }
}

bool StorylandDtzArchive::resizeCompanionImgForSectorPatch(
    uint32_t selectedStartSector,
    uint32_t selectedOldSectorCount,
    uint32_t selectedNewSectorCount,
    bool shiftLaterStarts,
    std::string& report,
    std::string& errorMessage
) {
    report.clear();

    if (imgRawData.empty()) {
        report = "Companion IMG not loaded; only GAME.DTZ sector metadata was patched.";
        return true;
    }

    int64_t deltaSectors = int64_t(selectedNewSectorCount) - int64_t(selectedOldSectorCount);
    if (deltaSectors == 0) {
        report = "Companion IMG loaded; sector count did not change, so IMG bytes were not moved.";
        return true;
    }

    if (!shiftLaterStarts) {
        report = "Companion IMG loaded. IMG resize skipped because start-sector shifting was off.";
        return true;
    }

    uint64_t startByte = uint64_t(selectedStartSector) * 2048ull;
    uint64_t oldEndSector64 = uint64_t(selectedStartSector) + uint64_t(selectedOldSectorCount);
    uint64_t newEndSector64 = uint64_t(selectedStartSector) + uint64_t(selectedNewSectorCount);
    uint64_t oldEndByte = oldEndSector64 * 2048ull;
    uint64_t newEndByte = newEndSector64 * 2048ull;
    if (startByte > imgRawData.size()) {
        errorMessage = "Selected sector start is past the loaded companion IMG size.";
        return false;
    }

    if (oldEndByte > imgRawData.size()) {
        uint64_t paddingNeeded = oldEndByte - uint64_t(imgRawData.size());
        if (paddingNeeded > 64ull * 1024ull * 1024ull) {
            errorMessage = "Selected sector range ends too far past the loaded IMG.";
            return false;
        }
        imgRawData.resize(size_t(oldEndByte), 0);
    }

    size_t oldSize = imgRawData.size();
    if (deltaSectors > 0) {
        uint64_t insertBytes64 = uint64_t(deltaSectors) * 2048ull;
        if (insertBytes64 > uint64_t(std::numeric_limits<size_t>::max())) {
            errorMessage = "Sector expansion is too large for this build.";
            return false;
        }
        imgRawData.insert(imgRawData.begin() + std::ptrdiff_t(oldEndByte), size_t(insertBytes64), uint8_t(0));
    } else {
        uint64_t eraseBytes64 = uint64_t(-deltaSectors) * 2048ull;
        if (newEndByte > oldEndByte || eraseBytes64 > oldEndByte - newEndByte) {
            errorMessage = "Invalid IMG shrink range.";
            return false;
        }
        imgRawData.erase(imgRawData.begin() + std::ptrdiff_t(newEndByte), imgRawData.begin() + std::ptrdiff_t(oldEndByte));
    }

    std::ostringstream ss;
    ss << "Companion IMG resized in memory: "
       << "start_sector=" << selectedStartSector
       << " old_count=" << selectedOldSectorCount
       << " new_count=" << selectedNewSectorCount
       << " delta_sectors=" << deltaSectors
       << " old_size=" << oldSize
       << " new_size=" << imgRawData.size()
       << ". Later file bytes physically moved by " << (deltaSectors * 2048ll) << " bytes.";
    report = ss.str();
    return true;
}

bool StorylandDtzArchive::saveToFile(const std::wstring& filePath, bool compressOutput, std::string& errorMessage) const {
    if (unpackedData.empty()) {
        errorMessage = "No DTZ data loaded.";
        return false;
    }
    if (!looksLikeRawDtz(unpackedData)) {
        errorMessage = "Not saving: edited raw DTZ buffer no longer begins with GTAG/GATG.";
        return false;
    }

    uint32_t declaredSize = readU32(unpackedData, 0x08);
    if (declaredSize != 0 && declaredSize != unpackedData.size()) {
        std::ostringstream ss;
        ss << "Not saving: DTZ header size "
           << declaredSize
           << " differs from edited raw size "
           << unpackedData.size()
           << ".";
        errorMessage = ss.str();
        return false;
    }

    std::vector<uint8_t> outputBytes;
    if (!compressOutput) {
        outputBytes = unpackedData;
    } else {
        if (!deflateZlibBytes(unpackedData, outputBytes, errorMessage)) return false;
        if (!validateDeflatedDtzRoundTrip(unpackedData, outputBytes, errorMessage)) return false;
    }

    if (!writeWholeFile(filePath, outputBytes, errorMessage)) return false;


    return writePatchedCompanionImg(filePath, errorMessage);
}

bool StorylandDtzArchive::parse(std::string& errorMessage) {
    headers.clear();
    records.clear();
    hints.clear();

    if (!looksLikeRawDtz(unpackedData)) {
        errorMessage = "Raw data does not begin with GTAG/GATG.";
        return false;
    }
    if (unpackedData.size() < 0xE0) {
        errorMessage = "DTZ header is too small.";
        return false;
    }

    addHeaderField(headers, "FourCC", 0x00, unpackedData, "GTAG/GATG raw DTZ signature after zlib inflate.");
    addHeaderField(headers, "VersionOrUnknown", 0x04, unpackedData, "Usually 1 for LCS/VCS DTZ.");
    addHeaderField(headers, "UnpackedDtzSize", 0x08, unpackedData, "Full raw DTZ size.");
    addHeaderField(headers, "DataSizeBeforeRelocationTables", 0x0C, unpackedData, "Data size before relocation/global tables.");
    addHeaderField(headers, "RelocationTable", 0x10, unpackedData, "Pointer relocation table offset.");
    addHeaderField(headers, "RelocationOffsetCount", 0x14, unpackedData, "Count of relocation offsets.");
    addHeaderField(headers, "VmtHashOffsetTable", 0x18, unpackedData, "Jenkins hash/VMT replacement table pointer.");
    addHeaderField(headers, "VmtHashCounts", 0x1C, unpackedData, "Two u16 counts packed in one dword.");
    addHeaderField(headers, "gpThePaths", 0x20, unpackedData, "Path data pointer.");
    addHeaderField(headers, "BuildingPoolIPL", 0x24, unpackedData, "Static building IPL-like data.");
    addHeaderField(headers, "TreadableIPL", 0x28, unpackedData, "Road/treadable IPL-like data.");
    addHeaderField(headers, "DummyIPL", 0x2C, unpackedData, "Dynamic dummy IPL-like data.");
    addHeaderField(headers, "EntryInfoNodes", 0x30, unpackedData, "Streaming dynamic object entry-info nodes.");
    addHeaderField(headers, "PtrNodes", 0x34, unpackedData, "Streaming map PtrNodes.");
    addHeaderField(headers, "IdeCount", 0x38, unpackedData, "Number of binary IDE pointers/records.");
    addHeaderField(headers, "IdePointerTable", 0x3C, unpackedData, "Table of offsets to binary IDE strings.");
    addHeaderField(headers, "VehicleClassTable", 0x40, unpackedData, "Vehicle class/model id table.");
    addHeaderField(headers, "VehicleClassItemCount", 0x44, unpackedData, "Count per vehicle class table row.");
    addHeaderField(headers, "ZoneData", 0x48, unpackedData, "info.zon/navig.zon-like data.");
    addHeaderField(headers, "CWorld_ms_aSectors", 0x4C, unpackedData, "Game world sector data.");
    addHeaderField(headers, "CWorld_ms_bigBuildingsList", 0x50, unpackedData, "Big building PtrNode list.");
    addHeaderField(headers, "TwoDfxCount", 0x54, unpackedData, "2dfx record count.");
    addHeaderField(headers, "TwoDfxTable", 0x58, unpackedData, "2dfx table, 64-byte rows.");
    addHeaderField(headers, "HardcodedModelIndexArray", 0x5C, unpackedData, "Hardcoded model indices.");
    addHeaderField(headers, "TexList", 0x60, unpackedData, "Texture archive loader block / TexList.");
    addHeaderField(headers, "UnknownZero_0x64", 0x64, unpackedData, "Usually zero.");
    addHeaderField(headers, "Col2LoaderBlock", 0x68, unpackedData, "COL2 loader block.");
    addHeaderField(headers, "UnknownZero_0x6C", 0x6C, unpackedData, "Usually zero.");
    addHeaderField(headers, "DummyCollision", 0x70, unpackedData, "Dummy collision.");
    addHeaderField(headers, "ObjectDat", 0x74, unpackedData, "object.dat analogue, documented as 32-byte rows.");
    addHeaderField(headers, "CarcolsPalette", 0x78, unpackedData, "RGBA palette used by carcols.dat.");
    addHeaderField(headers, "CStreamingInfo_ms_aInfoForModel", 0x7C, unpackedData, "GTA3PS*.DIR analogue; important for gta3PS2.img sector metadata.");
    addHeaderField(headers, "AnimLoaderBlock", 0x80, unpackedData, "ANIM/IFP loader block.");
    addHeaderField(headers, "fistfite.dat", 0x84, unpackedData, "fistfite.dat analogue.");
    addHeaderField(headers, "PedAnimInfo", 0x88, unpackedData, "PedAnimInfo table.");
    addHeaderField(headers, "ped.dat", 0x8C, unpackedData, "ped.dat analogue.");
    addHeaderField(headers, "pedstats.dat", 0x90, unpackedData, "pedstats.dat analogue.");
    addHeaderField(headers, "CullIplCountOrOffset", 0x94, unpackedData, "Documented cull.ipl count/related field; VCS/LCS may vary.");
    addHeaderField(headers, "CullIpl", 0x98, unpackedData, "cull.ipl analogue, documented as 16-byte rows.");
    addHeaderField(headers, "OcclusionUnused_0x9C", 0x9C, unpackedData, "Unused occlusion field, often zero.");
    addHeaderField(headers, "OcclusionUnused_0xA0", 0xA0, unpackedData, "Unused occlusion field, often zero.");
    addHeaderField(headers, "waterpro.dat", 0xA4, unpackedData, "waterpro.dat analogue.");
    addHeaderField(headers, "HANDLING.CFG", 0xA8, unpackedData, "handling data analogue.");
    addHeaderField(headers, "surface.dat", 0xAC, unpackedData, "surface.dat analogue.");
    addHeaderField(headers, "timecyc.dat", 0xB0, unpackedData, "timecyc.dat analogue.");
    addHeaderField(headers, "pedgrp.dat", 0xB4, unpackedData, "pedgrp.dat analogue.");
    addHeaderField(headers, "particle.cfg", 0xB8, unpackedData, "particle.cfg analogue.");
    addHeaderField(headers, "weapon.dat", 0xBC, unpackedData, "weapon.dat analogue.");
    addHeaderField(headers, "EmbeddedMdlClumpPointerTable", 0xC0, unpackedData, "Table of offsets to DTZ-embedded MDL clumps.");
    addHeaderField(headers, "CUTS.DIR", 0xC4, unpackedData, "CUTS.DIR analogue.");
    addHeaderField(headers, "ferry.dat", 0xC8, unpackedData, "ferry.dat analogue.");
    addHeaderField(headers, "tracks.dat/tracks2.dat", 0xCC, unpackedData, "tracks.dat and tracks2.dat analogue.");
    addHeaderField(headers, "flight.dat", 0xD0, unpackedData, "flight.dat analogue.");
    addHeaderField(headers, "menu.chk", 0xD4, unpackedData, "menu.chk compressed offset on documented LCS/VCS builds, may be zero/variant.");
    addHeaderField(headers, "fonts.chk real size", 0xD8, unpackedData, "fonts.chk real/unpacked size or variant field.");
    addHeaderField(headers, "fonts.chk", 0xDC, unpackedData, "fonts.chk compressed offset on documented LCS/VCS builds, may be zero/variant.");

    addHint(hints, unpackedData, "TexList / CHK loader block", 0x60, "DTZ header field 0x60");
    addHint(hints, unpackedData, "COL2 loader block", 0x68, "DTZ header field 0x68");
    addHint(hints, unpackedData, "CStreamingInfo ms_aInfoForModel[]", 0x7C, "Main GTA3PS2.IMG mapping analogue");
    addHint(hints, unpackedData, "ANIM loader block", 0x80, "DTZ header field 0x80");
    addHint(hints, unpackedData, "Embedded MDL clump pointer table", 0xC0, "DTZ-embedded MDL table");
    addHint(hints, unpackedData, "CUTS.DIR analogue", 0xC4, "DTZ header field 0xC4");
    addHint(hints, unpackedData, "menu.chk compressed", 0xD4, "Compressed CHK texture archive where present");
    addHint(hints, unpackedData, "fonts.chk compressed/size field", 0xD8, "Font CHK info field on documented builds");
    addHint(hints, unpackedData, "fonts.chk compressed", 0xDC, "Compressed CHK texture archive where present");

    rebuildSectorRecords();
    rebuildDataBlocksAndFields();
    rebuildDirMatches();
    return true;
}

void StorylandDtzArchive::rebuildSectorRecords() {
    records.clear();
    std::set<std::pair<uint32_t, uint32_t>> seenOffsets;

    auto addRecord = [&](uint32_t recordOffset, uint32_t startOffset, uint32_t countOffset, uint32_t start, uint32_t count, const std::string& source) {
        if (count == 0 || count > 0x20000) return;
        if (start == 0xFFFFFFFFu || count == 0xFFFFFFFFu) return;
        if (start > 0x400000) return;
        if (!seenOffsets.insert({startOffset, countOffset}).second) return;

        StorylandDtzSectorRecord record;
        record.recordOffset = recordOffset;
        record.startOffset = startOffset;
        record.countOffset = countOffset;
        record.startSector = start;
        record.sectorCount = count;
        record.source = source;
        record.resourceName = classifyKnownStreamingRecord(start, count);
        record.note = describeKnownStreamingRecord(start, count);
        records.push_back(record);
    };

    auto looksLikeStreamingEntry = [&](size_t offset) -> bool {
        if (offset + 24 > unpackedData.size()) return false;
        return readU32(unpackedData, offset + 0x00) == 0xAAAAAAAAu
            && readU32(unpackedData, offset + 0x04) == 0x00000000u
            && readU32(unpackedData, offset + 0x08) == 0x00000000u
            && readU32(unpackedData, offset + 0x0C) == 0xAAAA0000u;
    };


    uint32_t strictCount = 0;
    std::set<std::pair<uint32_t, uint32_t>> strictSectorPairs;
    for (size_t offset = 0; offset + 24 <= unpackedData.size(); offset += 4) {
        if (!looksLikeStreamingEntry(offset)) continue;

        uint32_t start = readU32(unpackedData, offset + 0x10);
        uint32_t count = readU32(unpackedData, offset + 0x14);
        if (!validStreamingPair(start, count)) continue;

        std::ostringstream source;
        source << "GAME.DTZ internal gta3PS2.img streaming table entry";
        addRecord(uint32_t(offset), uint32_t(offset + 0x10), uint32_t(offset + 0x14), start, count, source.str());
        strictSectorPairs.insert({start, count});
        strictCount++;
    }


    if (strictCount == 0) {
        for (size_t offset = 0; offset + 24 <= unpackedData.size(); offset += 4) {
            uint32_t a = readU32(unpackedData, offset + 0x00);
            uint32_t b = readU32(unpackedData, offset + 0x04);
            uint32_t c = readU32(unpackedData, offset + 0x08);
            uint32_t d = readU32(unpackedData, offset + 0x0C);
            uint32_t start = readU32(unpackedData, offset + 0x10);
            uint32_t count = readU32(unpackedData, offset + 0x14);
            if ((a == 0xAAAAAAAAu || a == 0x00000000u || a == 0xFFFFFFFFu) && b == 0 && c == 0 && (d == 0x0000AAAAu || d == 0xAAAAAAAAu || d == 0)) {
                if (validStreamingPair(start, count)) {
                    addRecord(uint32_t(offset), uint32_t(offset + 0x10), uint32_t(offset + 0x14), start, count, "loose 24-byte sector pair candidate");
                }
            }
        }
    }


    auto nameHasUsefulDtzExtension = [](const std::string& name) -> bool {
        if (name.empty()) return false;
        bool hasAlnum = false;
        for (char c : name) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                hasAlnum = true;
                break;
            }
        }
        if (!hasAlnum) return false;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        static const char* extensions[] = {
            ".mdl", ".xtx", ".chk", ".tex", ".anim", ".ifp", ".cut", ".dat", ".col", ".col2", ".wdr", ".wrld", ".raw", ".sdt", ".cam", ".gxt"
        };
        for (const char* ext : extensions) {
            if (lower.size() >= std::strlen(ext) && lower.rfind(ext) == lower.size() - std::strlen(ext)) {
                return true;
            }
        }
        return false;
    };


    if (strictCount == 0) {
        for (size_t offset = 0; offset + 32u <= unpackedData.size(); offset += 4u) {
            std::string name = readDirName(unpackedData, offset, 24u);
            if (!looksLikeDirName(name) || !nameHasUsefulDtzExtension(name)) {
                continue;
            }

            uint32_t start = readU32(unpackedData, offset + 24u);
            uint32_t count = readU32(unpackedData, offset + 28u);
            if (!validStreamingPair(start, count)) {
                continue;
            }

            if (!seenOffsets.insert({uint32_t(offset + 24u), uint32_t(offset + 28u)}).second) {
                continue;
            }

            StorylandDtzSectorRecord record;
            record.recordOffset = uint32_t(offset);
            record.startOffset = uint32_t(offset + 24u);
            record.countOffset = uint32_t(offset + 28u);
            record.startSector = start;
            record.sectorCount = count;
            record.source = "generic internal 32-byte ASCII DIR entry";
            record.resourceName = name;
            record.note = "Name + sector pair scanned from DTZ contents; no strict gta3PS2.img table was found.";
            records.push_back(record);
        }
    }

    std::sort(records.begin(), records.end(), [](const StorylandDtzSectorRecord& left, const StorylandDtzSectorRecord& right) {
        if (left.startSector != right.startSector) return left.startSector < right.startSector;
        if (left.sectorCount != right.sectorCount) return left.sectorCount < right.sectorCount;
        return left.recordOffset < right.recordOffset;
    });
}

static bool dtzValidOffset(const std::vector<uint8_t>& data, uint32_t offset) {
    return offset >= 0xE0 && offset < data.size();
}

static uint32_t dtzDataLimit(const std::vector<uint8_t>& data) {
    uint32_t declaredDataEnd = readU32(data, 0x0C);
    uint32_t relocationTable = readU32(data, 0x10);
    uint32_t limit = uint32_t(data.size());
    if (declaredDataEnd >= 0xE0 && declaredDataEnd < limit) limit = declaredDataEnd;
    if (relocationTable >= 0xE0 && relocationTable < limit) limit = relocationTable;
    return limit;
}

static std::string hexForValue(uint32_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return ss.str();
}

static std::string formatSignedInt(int32_t value) {
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

static bool parseUnsignedText(const std::string& text, uint64_t& valueOut) {
    errno = 0;
    char* end = nullptr;
    valueOut = std::strtoull(text.c_str(), &end, 0);
    if (errno != 0 || end == text.c_str()) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    return *end == '\0';
}

static bool parseSignedText(const std::string& text, int64_t& valueOut) {
    errno = 0;
    char* end = nullptr;
    valueOut = std::strtoll(text.c_str(), &end, 0);
    if (errno != 0 || end == text.c_str()) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    return *end == '\0';
}

static bool parseFloatText(const std::string& text, float& valueOut) {
    errno = 0;
    char* end = nullptr;
    double value = std::strtod(text.c_str(), &end);
    if (errno != 0 || end == text.c_str()) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (*end != '\0') return false;
    valueOut = static_cast<float>(value);
    return true;
}

static bool isPrintableAsciiByte(uint8_t value) {
    return value >= 0x20 && value <= 0x7E;
}

static bool hasAsciiNameAt(const std::vector<uint8_t>& data, uint32_t offset, size_t length, const char* prefix) {
    if (offset + length > data.size()) return false;
    std::string name = readFixedAscii(data, offset, length);
    if (name.empty()) return false;
    for (char c : name) {
        if (c != '.' && (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E)) return false;
    }
    if (prefix == nullptr || prefix[0] == '\0') return true;
    return name.rfind(prefix, 0) == 0;
}

static void addDtzDataField(
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block,
    uint32_t rowIndex,
    uint32_t absoluteOffset,
    uint32_t relativeOffset,
    uint32_t size,
    const std::string& rowLabel,
    const std::string& name,
    const std::string& type,
    const std::string& valueText,
    bool editable,
    const std::string& note
) {
    StorylandDtzDataField field;
    field.blockIndex = blockIndex;
    field.rowIndex = rowIndex;
    field.absoluteOffset = absoluteOffset;
    field.relativeOffset = relativeOffset;
    field.size = size;
    field.blockName = block.name;
    field.rowLabel = rowLabel;
    field.name = name;
    field.type = type;
    field.valueText = valueText;
    field.editable = editable;
    field.note = note;
    fields.push_back(field);
}

static std::string byteValueText(uint8_t value) {
    std::ostringstream ss;
    ss << unsigned(value) << " / 0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
    return ss.str();
}

static std::string u16ValueText(uint16_t value) {
    std::ostringstream ss;
    ss << value << " / 0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    return ss.str();
}

static std::string i16ValueText(int16_t value) {
    std::ostringstream ss;
    ss << value << " / 0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << uint16_t(value);
    return ss.str();
}

static std::string u32ValueText(uint32_t value) {
    std::ostringstream ss;
    ss << value << " / " << hexForValue(value);
    return ss.str();
}

static std::string i32ValueText(int32_t value) {
    std::ostringstream ss;
    ss << value << " / " << hexForValue(uint32_t(value));
    return ss.str();
}

static std::string f32ValueText(const std::vector<uint8_t>& data, uint32_t offset) {
    std::ostringstream ss;
    float value = readFloat32(data, offset);
    uint32_t raw = readU32(data, offset);
    ss << formatFloat32(value) << " / raw=" << hexForValue(raw);
    return ss.str();
}

static uint32_t clampRecordCountBySize(uint32_t offset, uint32_t recordSize, uint32_t wantedCount, uint32_t limit) {
    if (recordSize == 0 || offset >= limit) return 0;
    uint64_t maxCount = (uint64_t(limit) - uint64_t(offset)) / uint64_t(recordSize);
    if (maxCount > 0xFFFFFFFFull) maxCount = 0xFFFFFFFFull;
    if (wantedCount == 0 || wantedCount > maxCount) return uint32_t(maxCount);
    return wantedCount;
}

static uint32_t inferNextHeaderPointerEnd(const std::vector<uint8_t>& data, uint32_t start) {
    uint32_t limit = dtzDataLimit(data);
    uint32_t end = limit;
    for (uint32_t headerOffset = 0x20; headerOffset <= 0xDC; headerOffset += 4) {
        uint32_t candidate = readU32(data, headerOffset);
        if (candidate > start && candidate < end && candidate >= 0xE0 && candidate < limit) {
            end = candidate;
        }
    }
    return end;
}

static size_t appendDtzDataBlock(
    std::vector<StorylandDtzDataBlock>& blocks,
    const std::string& name,
    const std::string& parser,
    uint32_t headerOffset,
    uint32_t offset,
    uint32_t inferredEnd,
    uint32_t rowSize,
    uint32_t rowCount,
    bool editable,
    const std::string& note
) {
    StorylandDtzDataBlock block;
    block.name = name;
    block.parser = parser;
    block.headerOffset = headerOffset;
    block.offset = offset;
    block.inferredEnd = inferredEnd;
    block.rowSize = rowSize;
    block.rowCount = rowCount;
    if (rowSize != 0 && rowCount != 0) {
        uint64_t sized = uint64_t(rowSize) * uint64_t(rowCount);
        uint64_t maxSize = offset < inferredEnd ? uint64_t(inferredEnd - offset) : 0;
        block.size = uint32_t(std::min(sized, maxSize));
    } else {
        block.size = offset < inferredEnd ? inferredEnd - offset : 0;
    }
    block.editable = editable;
    block.note = note;
    blocks.push_back(block);
    return blocks.size() - 1;
}

static void appendGenericPreviewFields(
    const std::vector<uint8_t>& data,
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block,
    uint32_t maxRows
) {
    if (block.offset >= data.size() || block.rowSize == 0 || block.rowCount == 0) return;
    uint32_t rows = std::min(block.rowCount, maxRows);
    for (uint32_t row = 0; row < rows; ++row) {
        uint32_t rowOffset = block.offset + row * block.rowSize;
        if (rowOffset + block.rowSize > data.size()) break;
        std::ostringstream rowLabel;
        rowLabel << "record " << row;
        for (uint32_t rel = 0; rel + 4 <= block.rowSize; rel += 4) {
            uint32_t off = rowOffset + rel;
            std::ostringstream name;
            name << "+0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << rel;
            addDtzDataField(fields, blockIndex, block, row, off, rel, 4, rowLabel.str(), name.str() + " u32", "u32", u32ValueText(readU32(data, off)), block.editable, "generic u32 view/edit of this row slot");
            addDtzDataField(fields, blockIndex, block, row, off, rel, 4, rowLabel.str(), name.str() + " f32", "float", f32ValueText(data, off), block.editable, "same four bytes viewed as float");
        }
    }
}

static void appendCarcolsPaletteFields(
    const std::vector<uint8_t>& data,
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block
) {
    for (uint32_t row = 0; row < block.rowCount; ++row) {
        uint32_t base = block.offset + row * 4;
        if (base + 4 > data.size()) break;
        std::ostringstream rowLabel;
        rowLabel << "color " << row;
        addDtzDataField(fields, blockIndex, block, row, base + 0, 0, 1, rowLabel.str(), "red", "u8", byteValueText(data[base + 0]), true, "RGBA palette byte used by carcols-style data");
        addDtzDataField(fields, blockIndex, block, row, base + 1, 1, 1, rowLabel.str(), "green", "u8", byteValueText(data[base + 1]), true, "RGBA palette byte used by carcols-style data");
        addDtzDataField(fields, blockIndex, block, row, base + 2, 2, 1, rowLabel.str(), "blue", "u8", byteValueText(data[base + 2]), true, "RGBA palette byte used by carcols-style data");
        addDtzDataField(fields, blockIndex, block, row, base + 3, 3, 1, rowLabel.str(), "alpha", "u8", byteValueText(data[base + 3]), true, "RGBA palette byte used by carcols-style data");
    }
}

static void appendCullFields(
    const std::vector<uint8_t>& data,
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block
) {
    for (uint32_t row = 0; row < block.rowCount; ++row) {
        uint32_t base = block.offset + row * 16;
        if (base + 16 > data.size()) break;
        std::ostringstream rowLabel;
        rowLabel << "cull " << row;
        const char* names[8] = {"boxLowerX", "boxUpperX", "boxLowerY", "boxUpperY", "boxLowerZ", "boxUpperZ", "flag1", "flag2"};
        for (uint32_t i = 0; i < 8; ++i) {
            uint32_t off = base + i * 2;
            std::string type = i < 6 ? "i16" : "u16";
            std::string value = i < 6 ? i16ValueText(readI16(data, off)) : u16ValueText(readU16(data, off));
            addDtzDataField(fields, blockIndex, block, row, off, i * 2, 2, rowLabel.str(), names[i], type, value, true, "cull.ipl 16-byte record field");
        }
    }
}

static uint32_t countPedstatPointers(const std::vector<uint8_t>& data, uint32_t pointerTable) {
    if (!dtzValidOffset(data, pointerTable)) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < 128; ++i) {
        uint32_t ptr = readU32(data, pointerTable + i * 4);
        if (!dtzValidOffset(data, ptr) || ptr + 0x34 > data.size()) break;
        if (!hasAsciiNameAt(data, ptr + 0x1A, 26, "STAT_")) break;
        count++;
    }
    return count;
}

static void appendPedstatFields(
    const std::vector<uint8_t>& data,
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block
) {
    for (uint32_t row = 0; row < block.rowCount; ++row) {
        uint32_t recordPtrOffset = block.offset + row * 4;
        uint32_t base = readU32(data, recordPtrOffset);
        if (!dtzValidOffset(data, base) || base + 0x34 > data.size()) break;
        std::string name = readFixedAscii(data, base + 0x1A, 26);
        std::ostringstream rowLabel;
        rowLabel << row << " " << name;
        addDtzDataField(fields, blockIndex, block, row, recordPtrOffset, row * 4, 4, rowLabel.str(), "record pointer", "u32", u32ValueText(base), false, "pointer table entry; not edited here until relocation handling is safer");
        addDtzDataField(fields, blockIndex, block, row, base + 0x00, 0x00, 4, rowLabel.str(), "iNumber", "i32", i32ValueText(readI32(data, base + 0x00)), true, "VCS pedstats record");
        addDtzDataField(fields, blockIndex, block, row, base + 0x04, 0x04, 4, rowLabel.str(), "fFleeDistance", "float", f32ValueText(data, base + 0x04), true, "VCS pedstats record");
        addDtzDataField(fields, blockIndex, block, row, base + 0x08, 0x08, 4, rowLabel.str(), "fHeadingChangeRate", "float", f32ValueText(data, base + 0x08), true, "VCS pedstats record");
        addDtzDataField(fields, blockIndex, block, row, base + 0x0C, 0x0C, 4, rowLabel.str(), "fAttackStrength", "float", f32ValueText(data, base + 0x0C), true, "VCS pedstats record");
        addDtzDataField(fields, blockIndex, block, row, base + 0x10, 0x10, 4, rowLabel.str(), "fDefendWeakness", "float", f32ValueText(data, base + 0x10), true, "VCS pedstats record");
        addDtzDataField(fields, blockIndex, block, row, base + 0x14, 0x14, 1, rowLabel.str(), "btStatFlags", "u8", byteValueText(data[base + 0x14]), true, "VCS pedstats record byte");
        addDtzDataField(fields, blockIndex, block, row, base + 0x15, 0x15, 1, rowLabel.str(), "btZero", "u8", byteValueText(data[base + 0x15]), true, "VCS pedstats record byte");
        addDtzDataField(fields, blockIndex, block, row, base + 0x16, 0x16, 1, rowLabel.str(), "btFear", "u8", byteValueText(data[base + 0x16]), true, "VCS pedstats record byte");
        addDtzDataField(fields, blockIndex, block, row, base + 0x17, 0x17, 1, rowLabel.str(), "btTemper", "u8", byteValueText(data[base + 0x17]), true, "VCS pedstats record byte");
        addDtzDataField(fields, blockIndex, block, row, base + 0x18, 0x18, 1, rowLabel.str(), "btLawfulness", "u8", byteValueText(data[base + 0x18]), true, "VCS pedstats record byte");
        addDtzDataField(fields, blockIndex, block, row, base + 0x19, 0x19, 1, rowLabel.str(), "btView", "u8", byteValueText(data[base + 0x19]), true, "VCS pedstats record byte");
        addDtzDataField(fields, blockIndex, block, row, base + 0x1A, 0x1A, 26, rowLabel.str(), "name", "ascii", name, false, "fixed 26-byte name; view-only in this patch");
    }
}

static bool looksLikeParticleRow(const std::vector<uint8_t>& data, uint32_t offset) {
    if (offset + 24 > data.size()) return false;
    std::string name = readFixedAscii(data, offset + 4, 20);
    if (name.size() < 2) return false;
    int letters = 0;
    for (char c : name) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-' || (c >= '0' && c <= '9')) letters++;
    }
    return letters >= 2;
}

static void appendParticlePreviewFields(
    const std::vector<uint8_t>& data,
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block
) {
    uint32_t rows = std::min(block.rowCount, 128u);
    for (uint32_t row = 0; row < rows; ++row) {
        uint32_t base = block.offset + row * block.rowSize;
        if (base + block.rowSize > data.size()) break;
        if (!looksLikeParticleRow(data, base)) continue;
        std::string rowName = readFixedAscii(data, base + 4, 20);
        std::ostringstream rowLabel;
        rowLabel << row << " " << rowName;
        addDtzDataField(fields, blockIndex, block, row, base + 0x00, 0x00, 4, rowLabel.str(), "dwID", "u32", u32ValueText(readU32(data, base + 0x00)), true, "particle row ID");
        addDtzDataField(fields, blockIndex, block, row, base + 0x04, 0x04, 20, rowLabel.str(), "Name", "ascii", rowName, false, "particle name");
        for (uint32_t rel = 0x18; rel + 4 <= std::min(block.rowSize, 0x54u); rel += 4) {
            uint32_t off = base + rel;
            std::ostringstream name;
            name << "+0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << rel;
            addDtzDataField(fields, blockIndex, block, row, off, rel, 4, rowLabel.str(), name.str() + " f32/u32", "float", f32ValueText(data, off), true, "particle numeric field; exact VCS label may need more naming work");
        }
    }
}

static void appendTimecycFields(
    const std::vector<uint8_t>& data,
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block
) {
    if (block.rowSize == 36 && block.rowCount > 0) {
        static const char* channelNames[9] = {
            "ambient.r",
            "ambient.g",
            "ambient.b",
            "directional.r",
            "directional.g",
            "directional.b",
            "sky/top.r",
            "sky/top.g",
            "sky/top.b"
        };

        for (uint32_t row = 0; row < block.rowCount; ++row) {
            uint32_t base = block.offset + row * block.rowSize;
            if (base + block.rowSize > data.size()) break;

            uint32_t weather = row / 24u;
            uint32_t hour = row % 24u;
            std::ostringstream rowLabel;
            if (block.rowCount >= 24 && (block.rowCount % 24u) == 0) {
                rowLabel << "weather " << weather << " hour " << std::setw(2) << std::setfill('0') << hour;
            } else {
                rowLabel << "hour row " << row;
            }

            for (uint32_t channel = 0; channel < 9; ++channel) {
                uint32_t rel = channel * 4;
                std::ostringstream note;
                note << "3D-era timecyc-style compact hourly float channel. This GAME.DTZ block is 36 bytes per row: 9 float channels, grouped as three RGB triplets. Exact Stories channel names may still need confirmation; the layout is no longer shown as anonymous dwords.";
                addDtzDataField(fields, blockIndex, block, row, base + rel, rel, 4, rowLabel.str(), channelNames[channel], "float", f32ValueText(data, base + rel), true, note.str());
            }
        }
        return;
    }

    uint32_t dwordCount = block.size / 4;
    uint32_t shown = std::min(dwordCount, 512u);
    for (uint32_t i = 0; i < shown; ++i) {
        uint32_t off = block.offset + i * 4;
        if (off + 4 > data.size()) break;
        std::ostringstream rowLabel;
        rowLabel << "dword " << i << " +0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << (i * 4);
        addDtzDataField(fields, blockIndex, block, i, off, i * 4, 4, rowLabel.str(), "float value", "float", f32ValueText(data, off), true, "timecyc.dat raw dword viewed as float");
        addDtzDataField(fields, blockIndex, block, i, off, i * 4, 4, rowLabel.str(), "raw u32", "u32", u32ValueText(readU32(data, off)), true, "same timecyc.dat dword viewed as raw u32");
    }
}

static bool finiteFloatInRange(const std::vector<uint8_t>& data, uint32_t offset, float minValue, float maxValue) {
    if (offset + 4 > data.size()) return false;
    float value = readFloat32(data, offset);
    return std::isfinite(value) && value >= minValue && value <= maxValue;
}

static uint32_t scoreVcsWeaponRecord(const std::vector<uint8_t>& data, uint32_t offset) {
    if (offset + 0x70 > data.size()) return 0;
    uint32_t score = 0;
    int32_t fireType = readI32(data, offset + 0x04);
    int32_t firingRate = readI32(data, offset + 0x0C);
    int32_t reload = readI32(data, offset + 0x10);
    int32_t ammo = readI32(data, offset + 0x14);
    int32_t damage = readI32(data, offset + 0x18);
    uint32_t paddingA = readU32(data, offset + 0x2C);
    uint32_t paddingB = readU32(data, offset + 0x6C);
    uint32_t zero3C = readU32(data, offset + 0x3C);

    if (fireType >= 0 && fireType <= 8) score += 4;
    if (finiteFloatInRange(data, offset + 0x08, 0.0f, 1000.0f)) score += 5;
    if (firingRate >= 0 && firingRate <= 10000) score += 3;
    if (reload >= 0 && reload <= 10000) score += 3;
    if (ammo >= 0 && ammo <= 100000) score += 2;
    if (damage >= 0 && damage <= 5000) score += 3;
    if (finiteFloatInRange(data, offset + 0x1C, -10.0f, 10000.0f)) score += 2;
    if (finiteFloatInRange(data, offset + 0x20, -10.0f, 10000.0f)) score += 2;
    if (finiteFloatInRange(data, offset + 0x24, -10.0f, 10000.0f)) score += 2;
    if (finiteFloatInRange(data, offset + 0x28, -10.0f, 10000.0f)) score += 2;
    if (paddingA == 0xAAAAAAAAu) score += 8;
    if (paddingB == 0xAAAAAAAAu) score += 8;
    if (zero3C == 0) score += 2;
    return score;
}

static uint32_t scoreVcsWeaponBlock(const std::vector<uint8_t>& data, uint32_t offset, uint32_t count) {
    if (offset + uint64_t(count) * 0x70ull > data.size()) return 0;
    uint32_t total = 0;
    for (uint32_t row = 0; row < count; ++row) {
        uint32_t score = scoreVcsWeaponRecord(data, offset + row * 0x70);
        if (score < 20) return 0;
        total += score;
    }
    return total;
}

static bool findVcsWeaponBlock(const std::vector<uint8_t>& data, uint32_t& offsetOut, uint32_t& countOut, uint32_t& scoreOut) {
    uint32_t limit = dtzDataLimit(data);
    uint32_t bestOffset = 0;
    uint32_t bestCount = 0;
    uint32_t bestScore = 0;

    auto testCandidate = [&](uint32_t offset, uint32_t count) {
        if (!dtzValidOffset(data, offset) || offset + uint64_t(count) * 0x70ull > limit) return;
        uint32_t score = scoreVcsWeaponBlock(data, offset, count);
        if (score > bestScore) {
            bestScore = score;
            bestOffset = offset;
            bestCount = count;
        }
    };

    testCandidate(0x003D7530u, 40);
    testCandidate(0x0040A980u, 38);
    testCandidate(0x0044FAE0u, 38);
    testCandidate(readU32(data, 0xBC), 40);
    testCandidate(readU32(data, 0xBC), 38);

    for (uint32_t offset = 0xE0; offset + 0x70u * 38u <= limit; offset += 0x10) {
        if (readU32(data, offset + 0x2C) != 0xAAAAAAAAu) continue;
        if (!finiteFloatInRange(data, offset + 0x08, 0.0f, 1000.0f)) continue;
        testCandidate(offset, 40);
        testCandidate(offset, 38);
    }

    if (bestScore == 0) return false;
    offsetOut = bestOffset;
    countOut = bestCount;
    scoreOut = bestScore;
    return true;
}

static void appendVcsWeaponFields(
    const std::vector<uint8_t>& data,
    std::vector<StorylandDtzDataField>& fields,
    size_t blockIndex,
    const StorylandDtzDataBlock& block
) {
    for (uint32_t row = 0; row < block.rowCount; ++row) {
        uint32_t base = block.offset + row * 0x70;
        if (base + 0x70 > data.size()) break;

        int32_t weaponSlot = readI32(data, base + 0x68);
        int32_t modelId = readI32(data, base + 0x60);
        int32_t model2Id = readI32(data, base + 0x64);

        std::ostringstream rowLabel;
        rowLabel << "weapon " << std::setw(2) << std::setfill('0') << row
                 << "  slot=" << weaponSlot
                 << "  model=" << modelId;
        if (model2Id != -1) rowLabel << "/" << model2Id;

        auto addU32 = [&](uint32_t rel, const std::string& name, bool editable, const std::string& note) {
            addDtzDataField(fields, blockIndex, block, row, base + rel, rel, 4, rowLabel.str(), name, "u32", u32ValueText(readU32(data, base + rel)), editable, note);
        };
        auto addI32 = [&](uint32_t rel, const std::string& name, bool editable, const std::string& note) {
            addDtzDataField(fields, blockIndex, block, row, base + rel, rel, 4, rowLabel.str(), name, "i32", i32ValueText(readI32(data, base + rel)), editable, note);
        };
        auto addF32 = [&](uint32_t rel, const std::string& name, bool editable, const std::string& note) {
            addDtzDataField(fields, blockIndex, block, row, base + rel, rel, 4, rowLabel.str(), name, "float", f32ValueText(data, base + rel), editable, note);
        };

        addU32(0x00, "dwType / weapon type flags", true, "VCS weapon.dat leading DWORD; documented VCS-only field before iFireType");
        addI32(0x04, "iFireType", true, "weapon.dat fire type");
        addF32(0x08, "fRange", true, "weapon.dat range");
        addI32(0x0C, "iFiringRate", true, "weapon.dat firing rate");
        addI32(0x10, "iReload", true, "weapon.dat reload time");
        addI32(0x14, "iAmountOfAmmunition", true, "weapon.dat ammo amount");
        addI32(0x18, "iDamage", true, "weapon.dat damage");
        addF32(0x1C, "fSpeed", true, "weapon.dat projectile/speed value");
        addF32(0x20, "fRadius", true, "weapon.dat radius");
        addF32(0x24, "fLifeSpan", true, "weapon.dat lifespan");
        addF32(0x28, "fSpread", true, "weapon.dat spread");
        addU32(0x2C, "padding_0x2C", false, "padding/sentinel, normally 0xAAAAAAAA in the VCS PS2 block");
        addF32(0x30, "m_vFireOffset.x", true, "weapon.dat muzzle/fire offset vector x");
        addF32(0x34, "m_vFireOffset.y", true, "weapon.dat muzzle/fire offset vector y");
        addF32(0x38, "m_vFireOffset.z", true, "weapon.dat muzzle/fire offset vector z");
        addU32(0x3C, "_f3C / zero", true, "documented VCS unknown DWORD, usually zero");
        addU32(0x40, "_f40 / anim-or-effect id", true, "documented VCS integer field after fire offset");
        addU32(0x44, "_f44 / unknown dword", true, "documented VCS unknown DWORD");
        addF32(0x48, "m_vTimeStrc0.x", true, "weapon.dat timing vector 0 x");
        addF32(0x4C, "m_vTimeStrc0.y", true, "weapon.dat timing vector 0 y");
        addF32(0x50, "m_vTimeStrc0.z", true, "weapon.dat timing vector 0 z");
        addF32(0x54, "m_vTimeStrc1.x", true, "weapon.dat timing vector 1 x");
        addF32(0x58, "m_vTimeStrc1.y", true, "weapon.dat timing vector 1 y");
        addF32(0x5C, "m_vTimeStrc1.z", true, "weapon.dat timing vector 1 z");
        addI32(0x60, "dwModelID / documented unk slot", true, "In the tested VCS PS2 block this behaves like the primary model id; some old docs describe this offset as an unknown/float slot, so Storyland keeps the offset explicit");
        addI32(0x64, "dwModel2ID", true, "secondary model id; -1 means none/null on many records");
        addI32(0x68, "dwWeaponSlot", true, "weapon slot/index field visible in the tested VCS PS2 weapon table");
        addU32(0x6C, "padding_0x6C", false, "padding/sentinel, normally 0xAAAAAAAA in the tested VCS PS2 block");
    }
}

void StorylandDtzArchive::rebuildDataBlocksAndFields() {
    dataBlocksCache.clear();
    dataFieldsCache.clear();
    if (unpackedData.size() < 0xE0) return;

    const uint32_t limit = dtzDataLimit(unpackedData);
    std::set<uint32_t> addedRawOffsets;

    for (const StorylandDtzHeaderField& header : headers) {
        uint32_t value = header.value;
        if (!dtzValidOffset(unpackedData, value) || value >= limit) continue;
        if (!addedRawOffsets.insert(value).second) continue;
        uint32_t end = inferNextHeaderPointerEnd(unpackedData, value);
        std::ostringstream note;
        note << "real range derived from GTAG header field " << hexOffset(header.offset)
             << "; end is the next higher valid header pointer or relocation/data limit";
        appendDtzDataBlock(dataBlocksCache, header.name, "raw-scanned-header-pointer", header.offset, value, end, 4, (end > value ? (end - value) / 4 : 0), false, note.str());
    }

    auto findBlockByName = [&](const std::string& name) -> size_t {
        for (size_t i = 0; i < dataBlocksCache.size(); ++i) {
            if (dataBlocksCache[i].name == name) return i;
        }
        return size_t(-1);
    };

    uint32_t objectOffset = readU32(unpackedData, 0x74);
    if (dtzValidOffset(unpackedData, objectOffset)) {
        uint32_t count = clampRecordCountBySize(objectOffset, 32, 117, limit);
        size_t blockIndex = appendDtzDataBlock(dataBlocksCache, "object.dat records", "object.dat-32-byte-records", 0x74, objectOffset, std::min<uint32_t>(limit, objectOffset + count * 32), 32, count, true, "object.dat analogue parsed from real header pointer 0x74; documented 32-byte rows, 117 rows on known builds");
        appendGenericPreviewFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex], count);
    }

    uint32_t carcolsOffset = readU32(unpackedData, 0x78);
    if (dtzValidOffset(unpackedData, carcolsOffset)) {
        uint32_t count = clampRecordCountBySize(carcolsOffset, 4, 256, limit);
        size_t blockIndex = appendDtzDataBlock(dataBlocksCache, "carcols.dat RGBA palette", "carcols-rgba-palette", 0x78, carcolsOffset, std::min<uint32_t>(limit, carcolsOffset + count * 4), 4, count, true, "256 RGBA color table parsed from real header pointer 0x78 when present");
        appendCarcolsPaletteFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex]);
    }

    uint32_t cullA = readU32(unpackedData, 0x94);
    uint32_t cullB = readU32(unpackedData, 0x98);
    uint32_t cullOffset = 0;
    uint32_t cullCount = 0;
    if (dtzValidOffset(unpackedData, cullA) && cullB > 0 && cullB < 100000) {
        cullOffset = cullA;
        cullCount = cullB;
    } else if (dtzValidOffset(unpackedData, cullB) && cullA > 0 && cullA < 100000) {
        cullOffset = cullB;
        cullCount = cullA;
    }
    if (cullOffset != 0 && cullCount != 0) {
        cullCount = clampRecordCountBySize(cullOffset, 16, cullCount, limit);
        size_t blockIndex = appendDtzDataBlock(dataBlocksCache, "cull.ipl records", "cull-ipl-16-byte-records", dtzValidOffset(unpackedData, cullA) ? 0x94 : 0x98, cullOffset, std::min<uint32_t>(limit, cullOffset + cullCount * 16), 16, cullCount, true, "real cull.ipl range detected by finding which of header 0x94/0x98 is the pointer and which is the count");
        appendCullFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex]);
    }

    for (uint32_t headerOffset : {0x8Cu, 0x90u}) {
        uint32_t pointerTable = readU32(unpackedData, headerOffset);
        uint32_t count = countPedstatPointers(unpackedData, pointerTable);
        if (count == 0) continue;
        std::ostringstream name;
        name << "pedstats.dat STAT_* pointer table from header " << hexOffset(headerOffset);
        size_t blockIndex = appendDtzDataBlock(dataBlocksCache, name.str(), "vcs-pedstats-pointer-table", headerOffset, pointerTable, pointerTable + count * 4, 4, count, true, "actual content scan: pointer table entries lead to 0x34-byte records whose names begin with STAT_");
        appendPedstatFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex]);
    }

    for (uint32_t headerOffset : {0xB8u, 0xBCu}) {
        uint32_t offset = readU32(unpackedData, headerOffset);
        if (!dtzValidOffset(unpackedData, offset) || !looksLikeParticleRow(unpackedData, offset)) continue;
        uint32_t end = inferNextHeaderPointerEnd(unpackedData, offset);
        uint32_t rowSize = 0x54;
        uint32_t count = clampRecordCountBySize(offset, rowSize, 128, end);
        std::ostringstream name;
        name << "particle-style named rows from header " << hexOffset(headerOffset);
        size_t blockIndex = appendDtzDataBlock(dataBlocksCache, name.str(), "particle-named-row-scan", headerOffset, offset, end, rowSize, count, true, "actual content scan: rows begin with an id and a printable particle/effect name such as SPARK; some VCS field names are still generic");
        appendParticlePreviewFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex]);
    }

    uint32_t timecycOffset = readU32(unpackedData, 0xB0);
    if (dtzValidOffset(unpackedData, timecycOffset)) {
        uint32_t end = inferNextHeaderPointerEnd(unpackedData, timecycOffset);
        if (end > timecycOffset + 4) {
            uint32_t size = end - timecycOffset;
            if ((size % 36u) == 0 && (size / 36u) >= 24u && (size / 36u) <= 96u) {
                uint32_t rowCount = size / 36u;
                size_t blockIndex = appendDtzDataBlock(dataBlocksCache, "timecyc.dat hourly 3D-era compact rows", "timecyc-3d-era-36-byte-hourly-rows", 0xB0, timecycOffset, end, 36, rowCount, true, "real header pointer 0xB0; parsed as 36-byte hourly rows: 9 float channels, shown as three RGB triplets instead of anonymous dwords");
                appendTimecycFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex]);
            } else {
                uint32_t dwordCount = size / 4;
                size_t blockIndex = appendDtzDataBlock(dataBlocksCache, "timecyc.dat real scanned range", "timecyc-real-dword-view", 0xB0, timecycOffset, end, 4, dwordCount, true, "real header pointer 0xB0; fallback raw float/u32 view because this build does not match the 36-byte hourly compact layout");
                appendTimecycFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex]);
            }
        }
    }

    uint32_t weaponOffset = 0;
    uint32_t weaponCount = 0;
    uint32_t weaponScore = 0;
    if (findVcsWeaponBlock(unpackedData, weaponOffset, weaponCount, weaponScore)) {
        std::ostringstream note;
        note << "actual content scan for VCS/LCS weapon.dat-style 0x70-byte records; scanner score=" << weaponScore
             << "; header 0xBC can point at other row types on some VCS DTZs";
        size_t blockIndex = appendDtzDataBlock(dataBlocksCache, "weapon.dat real scanned records", "vcs-weapon-0x70-records", 0xBC, weaponOffset, weaponOffset + weaponCount * 0x70u, 0x70, weaponCount, true, note.str());
        appendVcsWeaponFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex]);
    }

    uint32_t handlingOffset = readU32(unpackedData, 0xA8);
    if (dtzValidOffset(unpackedData, handlingOffset)) {
        uint32_t end = inferNextHeaderPointerEnd(unpackedData, handlingOffset);
        uint32_t count = clampRecordCountBySize(handlingOffset, 0xE0, 64, end);
        size_t blockIndex = appendDtzDataBlock(dataBlocksCache, "handling/HANDLING.CFG real header range", "generic-0xE0-handling-view", 0xA8, handlingOffset, end, 0xE0, count, true, "real header pointer 0xA8; generic editable 0xE0-row view until the exact VCS subtable split is named");
        appendGenericPreviewFields(unpackedData, dataFieldsCache, blockIndex, dataBlocksCache[blockIndex], std::min(count, 32u));
    }

    std::sort(dataBlocksCache.begin(), dataBlocksCache.end(), [](const StorylandDtzDataBlock& left, const StorylandDtzDataBlock& right) {
        if (left.offset != right.offset) return left.offset < right.offset;
        return left.name < right.name;
    });


    std::vector<StorylandDtzDataBlock> sortedBlocks = dataBlocksCache;
    dataFieldsCache.clear();
    for (size_t blockIndex = 0; blockIndex < sortedBlocks.size(); ++blockIndex) {
        const StorylandDtzDataBlock& block = sortedBlocks[blockIndex];
        if (block.parser == "object.dat-32-byte-records") appendGenericPreviewFields(unpackedData, dataFieldsCache, blockIndex, block, block.rowCount);
        else if (block.parser == "carcols-rgba-palette") appendCarcolsPaletteFields(unpackedData, dataFieldsCache, blockIndex, block);
        else if (block.parser == "cull-ipl-16-byte-records") appendCullFields(unpackedData, dataFieldsCache, blockIndex, block);
        else if (block.parser == "vcs-pedstats-pointer-table") appendPedstatFields(unpackedData, dataFieldsCache, blockIndex, block);
        else if (block.parser == "particle-named-row-scan") appendParticlePreviewFields(unpackedData, dataFieldsCache, blockIndex, block);
        else if (block.parser == "timecyc-3d-era-36-byte-hourly-rows") appendTimecycFields(unpackedData, dataFieldsCache, blockIndex, block);
        else if (block.parser == "timecyc-real-dword-view") appendTimecycFields(unpackedData, dataFieldsCache, blockIndex, block);
        else if (block.parser == "vcs-weapon-0x70-records") appendVcsWeaponFields(unpackedData, dataFieldsCache, blockIndex, block);
        else if (block.parser == "generic-0xE0-handling-view") appendGenericPreviewFields(unpackedData, dataFieldsCache, blockIndex, block, std::min(block.rowCount, 32u));
        else if (block.parser == "raw-scanned-header-pointer") appendGenericPreviewFields(unpackedData, dataFieldsCache, blockIndex, block, std::min(block.rowCount, 24u));
    }
    dataBlocksCache = std::move(sortedBlocks);
}

bool StorylandDtzArchive::patchDataField(size_t fieldIndex, const std::string& newValueText, std::string& report, std::string& errorMessage) {
    if (fieldIndex >= dataFieldsCache.size()) {
        errorMessage = "Invalid DTZ data field index.";
        return false;
    }
    StorylandDtzDataField field = dataFieldsCache[fieldIndex];
    if (!field.editable) {
        errorMessage = "Selected DTZ data field is view-only.";
        return false;
    }
    if (field.absoluteOffset + field.size > unpackedData.size()) {
        errorMessage = "Selected DTZ data field is outside the unpacked GAME.DTZ bytes.";
        return false;
    }

    std::string oldValue = field.valueText;
    if (field.type == "u8") {
        uint64_t value = 0;
        if (!parseUnsignedText(newValueText, value) || value > 0xFF) {
            errorMessage = "Expected an unsigned 8-bit value, decimal or 0x hex.";
            return false;
        }
        unpackedData[field.absoluteOffset] = uint8_t(value);
    } else if (field.type == "u16") {
        uint64_t value = 0;
        if (!parseUnsignedText(newValueText, value) || value > 0xFFFF) {
            errorMessage = "Expected an unsigned 16-bit value, decimal or 0x hex.";
            return false;
        }
        writeU16(unpackedData, field.absoluteOffset, uint16_t(value));
    } else if (field.type == "i16") {
        int64_t value = 0;
        if (!parseSignedText(newValueText, value) || value < -32768 || value > 32767) {
            errorMessage = "Expected a signed 16-bit value.";
            return false;
        }
        writeU16(unpackedData, field.absoluteOffset, uint16_t(int16_t(value)));
    } else if (field.type == "u32") {
        uint64_t value = 0;
        if (!parseUnsignedText(newValueText, value) || value > 0xFFFFFFFFull) {
            errorMessage = "Expected an unsigned 32-bit value, decimal or 0x hex.";
            return false;
        }
        writeU32(unpackedData, field.absoluteOffset, uint32_t(value));
    } else if (field.type == "i32") {
        int64_t value = 0;
        if (!parseSignedText(newValueText, value) || value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
            errorMessage = "Expected a signed 32-bit value.";
            return false;
        }
        writeU32(unpackedData, field.absoluteOffset, uint32_t(int32_t(value)));
    } else if (field.type == "float") {
        float value = 0.0f;
        if (!parseFloatText(newValueText, value)) {
            errorMessage = "Expected a float value.";
            return false;
        }
        writeFloat32(unpackedData, field.absoluteOffset, value);
    } else {
        errorMessage = "This field type is not editable yet.";
        return false;
    }

    parse(errorMessage);
    if (!errorMessage.empty()) return false;

    std::ostringstream ss;
    ss << "Patched GAME.DTZ data field inside " << field.blockName
       << " / " << field.rowLabel
       << " / " << field.name
       << " at " << hexOffset(field.absoluteOffset)
       << " type=" << field.type
       << " old=" << oldValue
       << " new=" << newValueText
       << ". Save As / Rebuild GAME.DTZ writes the modified DTZ.";
    report = ss.str();
    return true;
}

bool StorylandDtzArchive::patchRawBytes(uint32_t absoluteOffset, const std::vector<uint8_t>& bytes, std::string& report, std::string& errorMessage) {
    if (bytes.empty()) {
        errorMessage = "No bytes supplied.";
        return false;
    }
    if (absoluteOffset + bytes.size() > unpackedData.size()) {
        errorMessage = "Raw byte patch is outside the unpacked GAME.DTZ bytes.";
        return false;
    }
    std::copy(bytes.begin(), bytes.end(), unpackedData.begin() + absoluteOffset);
    parse(errorMessage);
    if (!errorMessage.empty()) return false;
    std::ostringstream ss;
    ss << "Patched " << bytes.size() << " raw byte(s) at " << hexOffset(absoluteOffset) << ".";
    report = ss.str();
    return true;
}

bool StorylandDtzArchive::patchSectorRecord(size_t recordIndex, uint32_t newSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage) {
    if (recordIndex >= records.size()) {
        errorMessage = "Invalid sector record index.";
        return false;
    }
    StorylandDtzSectorRecord selected = records[recordIndex];
    if (newSectorCount == 0) {
        errorMessage = "New sector count must be non-zero.";
        return false;
    }

    std::vector<uint8_t> originalUnpackedData = unpackedData;
    std::vector<uint8_t> originalImgRawData = imgRawData;
    std::vector<StorylandDtzDirEntry> originalExternalDirMap = externalDirMap;

    int64_t delta = int64_t(newSectorCount) - int64_t(selected.sectorCount);
    uint32_t oldEnd = selected.startSector + selected.sectorCount;
    uint32_t patchedCount = 0;
    uint32_t shiftedStarts = 0;

    writeU32(unpackedData, selected.countOffset, newSectorCount);
    patchedCount++;

    if (shiftLaterStarts && delta != 0) {
        for (const StorylandDtzSectorRecord& record : records) {
            if (record.startOffset == selected.startOffset && record.countOffset == selected.countOffset) continue;
            if (record.startSector >= oldEnd) {
                int64_t shifted = int64_t(record.startSector) + delta;
                if (shifted < 0 || shifted > 0xFFFFFFFFLL) continue;
                writeU32(unpackedData, record.startOffset, uint32_t(shifted));
                shiftedStarts++;
            }
        }
    }

    patchCompanionDirMap(selected.startSector, selected.sectorCount, newSectorCount, shiftLaterStarts);

    std::string imgReport;
    if (!resizeCompanionImgForSectorPatch(selected.startSector, selected.sectorCount, newSectorCount, shiftLaterStarts, imgReport, errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        return false;
    }

    if (!parse(errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        return false;
    }

    std::ostringstream ss;
    ss << "Patched record at " << hexOffset(selected.recordOffset);
    if (!selected.resourceName.empty()) ss << " (" << selected.resourceName << ")";
    ss << ": start=" << selected.startSector
       << " old_count=" << selected.sectorCount
       << " new_count=" << newSectorCount
       << " old_bytes=" << (selected.sectorCount * 2048u)
       << " new_bytes=" << (newSectorCount * 2048u)
       << " delta=" << delta
       << ". Updated counts=" << patchedCount
       << ", shifted later starts=" << shiftedStarts << ".";
    if (!imgReport.empty()) ss << "\r\n" << imgReport;
    report = ss.str();
    return true;
}

bool StorylandDtzArchive::patchDirEntry(size_t dirEntryIndex, uint32_t newSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage) {
    if (dirEntryIndex >= dirMap.size()) {
        errorMessage = "Invalid internal streaming entry index.";
        return false;
    }
    if (newSectorCount == 0) {
        errorMessage = "New sector count must be non-zero.";
        return false;
    }

    rebuildSectorRecords();
    rebuildDirMatches();

    if (dirEntryIndex >= dirMap.size()) {
        errorMessage = "Internal streaming entry changed while rebuilding matches.";
        return false;
    }

    StorylandDtzDirEntry selected = dirMap[dirEntryIndex];
    if (selected.matchingRecordIndices.empty()) {
        std::ostringstream ss;
        ss << selected.name << " is not backed by a recognized editable GAME.DTZ start/count record.";
        errorMessage = ss.str();
        return false;
    }

    std::vector<uint8_t> originalUnpackedData = unpackedData;
    std::vector<uint8_t> originalImgRawData = imgRawData;
    std::vector<StorylandDtzDirEntry> originalExternalDirMap = externalDirMap;

    int64_t delta = int64_t(newSectorCount) - int64_t(selected.sectorCount);
    uint32_t oldEnd = selected.startSector + selected.sectorCount;
    uint32_t patchedCounts = 0;
    uint32_t shiftedStarts = 0;

    for (size_t recordIndex : selected.matchingRecordIndices) {
        if (recordIndex >= records.size()) continue;
        const StorylandDtzSectorRecord& record = records[recordIndex];
        if (record.startSector == selected.startSector) {
            writeU32(unpackedData, record.countOffset, newSectorCount);
            patchedCounts++;
        }
    }

    if (shiftLaterStarts && delta != 0) {
        for (const StorylandDtzDirEntry& entry : dirMap) {
            if (entry.dirIndex == selected.dirIndex) continue;
            if (entry.startSector < oldEnd) continue;
            for (size_t recordIndex : entry.matchingRecordIndices) {
                if (recordIndex >= records.size()) continue;
                const StorylandDtzSectorRecord& record = records[recordIndex];
                if (record.startSector != entry.startSector) continue;
                int64_t shifted = int64_t(record.startSector) + delta;
                if (shifted < 0 || shifted > 0xFFFFFFFFLL) continue;
                writeU32(unpackedData, record.startOffset, uint32_t(shifted));
                shiftedStarts++;
            }
        }
    }

    patchCompanionDirMap(selected.startSector, selected.sectorCount, newSectorCount, shiftLaterStarts);

    std::string imgReport;
    if (!resizeCompanionImgForSectorPatch(selected.startSector, selected.sectorCount, newSectorCount, shiftLaterStarts, imgReport, errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        return false;
    }

    if (!parse(errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        return false;
    }
    rebuildDirMatches();

    std::ostringstream ss;
    ss << "Patched " << selected.name
       << ": start=" << selected.startSector
       << " old_count=" << selected.sectorCount
       << " new_count=" << newSectorCount
       << " old_bytes=" << (selected.sectorCount * 2048u)
       << " new_bytes=" << (newSectorCount * 2048u)
       << " delta=" << delta
       << ". DTZ count fields updated=" << patchedCounts
       << ", later DTZ start fields shifted=" << shiftedStarts
       << ". Save As writes the rebuilt GAME.DTZ";
    if (!imgRawData.empty()) ss << " and companion IMG";
    ss << ".";
    if (!imgReport.empty()) ss << "\r\n" << imgReport;
    report = ss.str();
    return true;
}

bool StorylandDtzArchive::replaceDirEntryBytes(size_t dirEntryIndex, const std::vector<uint8_t>& replacementBytes, bool shiftLaterStarts, std::string& report, std::string& errorMessage) {
    if (dirEntryIndex >= dirMap.size()) {
        errorMessage = "Invalid internal streaming entry index.";
        return false;
    }
    if (replacementBytes.empty()) {
        errorMessage = "Replacement file is empty.";
        return false;
    }
    if (imgRawData.empty()) {
        errorMessage = "No companion IMG is loaded, so Storyland cannot replace the internal IMG entry bytes.";
        return false;
    }

    uint64_t newSectorCount64 = (uint64_t(replacementBytes.size()) + 2047ull) / 2048ull;
    if (newSectorCount64 == 0 || newSectorCount64 > 0xFFFFFFFFull) {
        errorMessage = "Replacement file is too large for a 32-bit sector count.";
        return false;
    }
    uint32_t newSectorCount = uint32_t(newSectorCount64);


    StorylandDtzDirEntry selected = dirMap[dirEntryIndex];
    if (selected.sectorCount == 0) {
        errorMessage = "Selected internal IMG entry has a zero sector count.";
        return false;
    }

    uint64_t selectedStartByte64 = uint64_t(selected.startSector) * 2048ull;
    uint64_t selectedOldBudget64 = uint64_t(selected.sectorCount) * 2048ull;
    if (selectedStartByte64 > uint64_t(imgRawData.size())) {
        errorMessage = "Selected internal IMG entry starts outside the loaded companion IMG.";
        return false;
    }
    if (selectedOldBudget64 > uint64_t(std::numeric_limits<size_t>::max())) {
        errorMessage = "Selected internal IMG entry is too large for this build.";
        return false;
    }


    rebuildSectorRecords();

    std::vector<size_t> selectedRecordIndices;
    for (size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
        const StorylandDtzSectorRecord& record = records[recordIndex];
        if (record.startSector == selected.startSector && record.sectorCount == selected.sectorCount) {
            selectedRecordIndices.push_back(recordIndex);
        }
    }

    if (selectedRecordIndices.empty()) {
        std::ostringstream ss;
        ss << selected.name
           << " start=" << selected.startSector
           << " count=" << selected.sectorCount
           << " is not backed by a recognized editable GAME.DTZ start/count record.";
        errorMessage = ss.str();
        return false;
    }

    std::vector<uint8_t> originalUnpackedData = unpackedData;
    std::vector<uint8_t> originalImgRawData = imgRawData;
    std::vector<StorylandDtzDirEntry> originalExternalDirMap = externalDirMap;

    int64_t delta = int64_t(newSectorCount) - int64_t(selected.sectorCount);
    uint32_t oldEnd = selected.startSector + selected.sectorCount;
    uint32_t patchedCounts = 0;
    uint32_t shiftedStarts = 0;

    for (size_t recordIndex : selectedRecordIndices) {
        if (recordIndex >= records.size()) continue;
        const StorylandDtzSectorRecord& record = records[recordIndex];
        writeU32(unpackedData, record.countOffset, newSectorCount);
        patchedCounts++;
    }

    if (patchedCounts == 0) {
        errorMessage = "Selected internal IMG entry has no matching GAME.DTZ count field to patch.";
        return false;
    }

    if (shiftLaterStarts && delta != 0) {
        for (const StorylandDtzSectorRecord& record : records) {
            if (record.startSector == selected.startSector && record.sectorCount == selected.sectorCount) {
                continue;
            }
            if (record.startSector < oldEnd) {
                continue;
            }

            int64_t shifted = int64_t(record.startSector) + delta;
            if (shifted < 0 || shifted > 0xFFFFFFFFLL) {
                continue;
            }

            writeU32(unpackedData, record.startOffset, uint32_t(shifted));
            shiftedStarts++;
        }
    }

    patchCompanionDirMap(selected.startSector, selected.sectorCount, newSectorCount, shiftLaterStarts);

    std::string imgResizeReport;
    if (!resizeCompanionImgForSectorPatch(selected.startSector, selected.sectorCount, newSectorCount, shiftLaterStarts, imgResizeReport, errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        return false;
    }

    uint64_t startByte64 = uint64_t(selected.startSector) * 2048ull;
    uint64_t newBudgetBytes64 = uint64_t(newSectorCount) * 2048ull;
    uint64_t endByte64 = startByte64 + newBudgetBytes64;
    if (startByte64 > uint64_t(imgRawData.size()) || endByte64 > uint64_t(imgRawData.size())) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        errorMessage = "Resized companion IMG does not contain the selected replacement byte range.";
        return false;
    }
    if (newBudgetBytes64 > uint64_t(std::numeric_limits<size_t>::max())) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        errorMessage = "Replacement sector budget is too large for this build.";
        return false;
    }

    size_t startByte = size_t(startByte64);
    size_t endByte = size_t(endByte64);
    std::fill(imgRawData.begin() + std::ptrdiff_t(startByte), imgRawData.begin() + std::ptrdiff_t(endByte), uint8_t(0));
    std::copy(replacementBytes.begin(), replacementBytes.end(), imgRawData.begin() + std::ptrdiff_t(startByte));

    if (!parse(errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        return false;
    }
    rebuildDirMatches();

    std::ostringstream ss;
    ss << "Replaced " << selected.name
       << ": start=" << selected.startSector
       << " old_count=" << selected.sectorCount
       << " new_count=" << newSectorCount
       << " replacement_bytes=" << replacementBytes.size()
       << " padded_budget_bytes=" << (uint64_t(newSectorCount) * 2048ull)
       << " delta_sectors=" << delta
       << ". DTZ count fields updated=" << patchedCounts
       << ", later DTZ start fields shifted=" << shiftedStarts
       << ". Replacement bytes were written into the loaded companion IMG in memory."
       << "\r\nTarget was resolved by the selected start/count pair before rebuilding the browser list, so the replacement no longer drifts to the row above/below."
       << "\r\nSave As / Rebuild GAME.DTZ writes the rebuilt GAME.DTZ and companion IMG.";
    if (!imgResizeReport.empty()) ss << "\r\n" << imgResizeReport;
    report = ss.str();
    return true;
}

bool StorylandDtzArchive::patchExactSectorPair(uint32_t oldStartSector, uint32_t oldSectorCount, uint32_t newSectorCount, bool shiftLaterStarts, std::string& report, std::string& errorMessage) {
    if (newSectorCount == 0) {
        errorMessage = "New sector count must be non-zero.";
        return false;
    }

    std::vector<uint8_t> originalUnpackedData = unpackedData;
    std::vector<uint8_t> originalImgRawData = imgRawData;
    std::vector<StorylandDtzDirEntry> originalExternalDirMap = externalDirMap;

    int64_t delta = int64_t(newSectorCount) - int64_t(oldSectorCount);
    uint32_t oldEnd = oldStartSector + oldSectorCount;
    uint32_t patchedCounts = 0;
    uint32_t shiftedStarts = 0;

    rebuildSectorRecords();
    for (const StorylandDtzSectorRecord& record : records) {
        if (record.startSector == oldStartSector && record.sectorCount == oldSectorCount) {
            writeU32(unpackedData, record.countOffset, newSectorCount);
            patchedCounts++;
        }
    }

    if (patchedCounts == 0) {
        errorMessage = "No matching sector pair was found.";
        return false;
    }

    if (shiftLaterStarts && delta != 0) {
        rebuildSectorRecords();
        for (const StorylandDtzSectorRecord& record : records) {
            if (record.startSector == oldStartSector && record.sectorCount == newSectorCount) continue;
            if (record.startSector >= oldEnd) {
                int64_t shifted = int64_t(record.startSector) + delta;
                if (shifted < 0 || shifted > 0xFFFFFFFFLL) continue;
                writeU32(unpackedData, record.startOffset, uint32_t(shifted));
                shiftedStarts++;
            }
        }
    }

    patchCompanionDirMap(oldStartSector, oldSectorCount, newSectorCount, shiftLaterStarts);

    std::string imgReport;
    if (!resizeCompanionImgForSectorPatch(oldStartSector, oldSectorCount, newSectorCount, shiftLaterStarts, imgReport, errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        return false;
    }

    if (!parse(errorMessage)) {
        unpackedData = std::move(originalUnpackedData);
        imgRawData = std::move(originalImgRawData);
        externalDirMap = std::move(originalExternalDirMap);
        std::string ignored;
        parse(ignored);
        rebuildDirMatches();
        return false;
    }

    std::ostringstream ss;
    ss << "Patched exact sector pair start=" << oldStartSector
       << " old_count=" << oldSectorCount
       << " new_count=" << newSectorCount
       << " old_bytes=" << (oldSectorCount * 2048u)
       << " new_bytes=" << (newSectorCount * 2048u)
       << " delta=" << delta
       << ". Matching count fields=" << patchedCounts
       << ", shifted later starts=" << shiftedStarts << ".";
    if (!imgReport.empty()) ss << "\r\n" << imgReport;
    report = ss.str();
    return true;
}

bool StorylandDtzArchive::extractDirEntryBytes(size_t dirEntryIndex, std::vector<uint8_t>& bytes, std::string& errorMessage) const {
    if (dirEntryIndex >= dirMap.size()) {
        errorMessage = "DIR entry index is out of range.";
        return false;
    }
    if (imgRawData.empty()) {
        errorMessage = "No companion IMG is loaded for this GAME.DTZ.";
        return false;
    }

    const auto& entry = dirMap[dirEntryIndex];
    uint64_t startByte = uint64_t(entry.startSector) * 2048ull;
    uint64_t budgetBytes = uint64_t(entry.sectorCount) * 2048ull;
    if (startByte > uint64_t(imgRawData.size())) {
        errorMessage = "DIR entry start sector is outside the loaded IMG.";
        return false;
    }
    if (budgetBytes > uint64_t(std::numeric_limits<size_t>::max())) {
        errorMessage = "DIR entry is too large to extract on this build.";
        return false;
    }

    bytes.assign(size_t(budgetBytes), uint8_t(0));
    size_t available = imgRawData.size() - size_t(startByte);
    size_t copyBytes = std::min<size_t>(available, size_t(budgetBytes));
    if (copyBytes != 0) {
        std::copy(
            imgRawData.begin() + std::ptrdiff_t(startByte),
            imgRawData.begin() + std::ptrdiff_t(startByte + copyBytes),
            bytes.begin()
        );
    }
    return true;
}

bool StorylandDtzArchive::findDirEntryByStemAndExtension(const std::wstring& stem, const std::vector<std::wstring>& extensions, size_t& outIndex) const {
    std::wstring wantedStem = lowercaseWide(stem);
    std::vector<std::wstring> wantedExtensions;
    wantedExtensions.reserve(extensions.size());
    for (const auto& extension : extensions) wantedExtensions.push_back(lowercaseWide(extension));

    for (size_t index = 0; index < dirMap.size(); ++index) {
        std::wstring entryName = canonicalDtzDirEntryWideName(asciiWide(dirMap[index].name));
        if (widePathStemLower(entryName) != wantedStem) continue;
        std::wstring entryExtension = widePathExtensionLower(entryName);
        for (const auto& extension : wantedExtensions) {
            if (entryExtension == extension) {
                outIndex = index;
                return true;
            }
        }
    }
    return false;
}

const std::vector<StorylandDtzHeaderField>& StorylandDtzArchive::headerFields() const { return headers; }
const std::vector<StorylandDtzSectorRecord>& StorylandDtzArchive::sectorRecords() const { return records; }
const std::vector<StorylandDtzResourceHint>& StorylandDtzArchive::resourceHints() const { return hints; }
const std::vector<StorylandDtzDirEntry>& StorylandDtzArchive::dirEntries() const { return dirMap; }
const std::vector<StorylandDtzDataBlock>& StorylandDtzArchive::dataBlocks() const { return dataBlocksCache; }
const std::vector<StorylandDtzDataField>& StorylandDtzArchive::dataFields() const { return dataFieldsCache; }
const std::vector<uint8_t>& StorylandDtzArchive::unpackedBytes() const { return unpackedData; }
const std::wstring& StorylandDtzArchive::sourcePath() const { return path; }
const std::wstring& StorylandDtzArchive::companionDirPath() const { return dirPath; }
const std::wstring& StorylandDtzArchive::companionImgPath() const { return imgPath; }
uint64_t StorylandDtzArchive::companionImgSize() const { return uint64_t(imgRawData.size()); }
bool StorylandDtzArchive::wasCompressedInput() const { return compressedInput; }
bool StorylandDtzArchive::hasCompanionDir() const { return !externalDirMap.empty(); }
bool StorylandDtzArchive::hasCompanionImg() const { return !imgRawData.empty(); }
