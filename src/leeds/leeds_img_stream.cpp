#include "src/leeds/leeds_img_stream.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

namespace {

constexpr uint64_t sectorSize = 2048;
constexpr size_t scanBlockSectors = 512;
constexpr size_t transferBytes = 1024 * 1024;

uint32_t readLittleU32(const uint8_t* data) {
    return uint32_t(data[0]) | (uint32_t(data[1]) << 8) |
           (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}

std::string lowerCase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return char(std::tolower(c));
    });
    return value;
}

std::string extensionForSignature(const uint8_t* data) {
    const uint32_t value = readLittleU32(data);
    if (value == 0x006d646c) return ".mdl";
    if (value == 0x00746578) return ".xtx";
    if (value == 0x616e696d) return ".anim";
    if (value == 0x636f6c32 || value == 0x324c4f43) return ".col2";
    if (value == 0x10 && readLittleU32(data + 8) != 0) return ".dff";
    if (value == 0x16 && readLittleU32(data + 8) != 0) return ".txd";
    return "";
}

std::string filenameOnly(const std::string& name) {
    const auto start = name.find_last_of("/\\");
    std::string result = start == std::string::npos ? name : name.substr(start + 1);
    if (result.empty() || result == "." || result == "..") return "";
    for (char& c : result) {
        if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
    }
    return result;
}

}

bool StorylandImgStream::open(const std::filesystem::path& path,
                              const std::vector<StorylandImgNamedRange>& nativeEntries,
                              const std::filesystem::path& companionDir,
                              std::string& error) {
    source.clear(); sourceSize = 0; summary.clear(); fileEntries.clear();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        error = "IMG file not found.";
        return false;
    }
    sourceSize = std::filesystem::file_size(path, ec);
    if (ec || sourceSize == 0) {
        error = "Can't read IMG size.";
        return false;
    }
    source = path;

    if (!nativeEntries.empty() && loadNativeEntries(nativeEntries, error)) return true;

    std::filesystem::path directory = companionDir;
    if (directory.empty()) {
        const std::string stem = lowerCase(path.stem().string());
        const std::filesystem::path folder = path.parent_path();
        for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
            if (ec) break;
            if (!item.is_regular_file(ec)) continue;
            if (lowerCase(item.path().extension().string()) != ".dir") continue;
            const std::string candidateStem = lowerCase(item.path().stem().string());
            if (candidateStem == stem || ((stem == "gta3" || stem == "gta3psphr") && candidateStem == "gta3")) {
                directory = item.path();
                break;
            }
        }
    }
    if (!directory.empty() && loadDirectoryEntries(directory, error)) return true;
    if (scanLeedsEntries(error)) return true;

    // Unknown files must still be inspectable without pretending their sector
    // layout has been identified. The single raw row is deliberately unnamed.
    StorylandImgNamedRange fallback;
    fallback.name = "IMG bytes";
    fallback.startSector = 0;
    fallback.sectorCount = uint32_t(std::min<uint64_t>(sourceSize / sectorSize,
        std::numeric_limits<uint32_t>::max()));
    if (fallback.sectorCount == 0) {
        error = "IMG is smaller than one 2048-byte sector.";
        return false;
    }
    appendEntry(fallback, false);
    fileEntries.back().byteSize = sourceSize;
    summary = "Unknown IMG layout. Raw bytes are available. GAME.DTZ is needed for the retail file list.";
    error.clear();
    return true;
}

bool StorylandImgStream::appendEntry(const StorylandImgNamedRange& row, bool hasName) {
    if (row.sectorCount == 0) return false;
    const uint64_t offset = uint64_t(row.startSector) * sectorSize;
    const uint64_t length = uint64_t(row.sectorCount) * sectorSize;
    if (offset >= sourceSize || length > sourceSize - offset) return false;
    StorylandImgStreamEntry entry;
    entry.name = filenameOnly(row.name);
    entry.startSector = row.startSector;
    entry.sectorCount = row.sectorCount;
    entry.byteOffset = offset;
    entry.byteSize = length;
    entry.extension = lowerCase(std::filesystem::path(entry.name).extension().string());
    entry.hasOriginalName = hasName && !row.name.empty();
    if (entry.name.empty()) {
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "sector_%08u.bin", row.startSector);
        entry.name = buffer;
    }
    fileEntries.push_back(std::move(entry));
    return true;
}

bool StorylandImgStream::loadNativeEntries(
    const std::vector<StorylandImgNamedRange>& nativeEntries, std::string& error) {
    std::vector<StorylandImgStreamEntry> saved;
    saved.swap(fileEntries);
    fileEntries.reserve(nativeEntries.size());
    for (const auto& row : nativeEntries) appendEntry(row, true);
    if (fileEntries.empty()) {
        error = "GAME.DTZ has no sectors within this IMG.";
        fileEntries.swap(saved);
        return false;
    }
    // The authoritative DTZ table provides the sector allocations and original
    // names. Preserve that order, but check for an accidentally mismatched IMG.
    size_t known = 0, matched = 0;
    for (const auto& entry : fileEntries) {
        if (known >= 64) break;
        const std::string actual = detectExtension(entry.byteOffset);
        if (actual.empty()) continue;
        ++known;
        if (actual == entry.extension ||
            ((actual == ".xtx" || actual == ".txd") &&
             (entry.extension == ".chk" || entry.extension == ".tex"))) ++matched;
    }
    if (known >= 8 && matched == 0) {
        fileEntries.clear();
        fileEntries.swap(saved);
        error = "GAME.DTZ appears to describe a different IMG.";
        return false;
    }
    summary = "GAME.DTZ: " + std::to_string(fileEntries.size()) +
        " files, 2048-byte sectors. Names and sector ranges follow GAME.DTZ where present.";
    error.clear();
    return true;
}

bool StorylandImgStream::loadDirectoryEntries(const std::filesystem::path& directory,
                                               std::string& error) {
    std::error_code ec;
    const uint64_t length = std::filesystem::file_size(directory, ec);
    if (ec || length < 32 || length % 32 != 0 || length > 32ull * 1000000ull) return false;
    std::ifstream file(directory, std::ios::binary);
    if (!file) return false;
    std::vector<StorylandImgStreamEntry> saved;
    saved.swap(fileEntries);
    std::array<uint8_t, 32> buffer{};
    size_t valid = 0, rows = 0;
    while (file.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
        ++rows;
        const uint32_t start = readLittleU32(buffer.data());
        const uint32_t count = readLittleU32(buffer.data() + 4);
        std::string name;
        for (size_t i = 8; i < buffer.size() && buffer[i]; ++i) {
            if (buffer[i] < 32 || buffer[i] > 126) { name.clear(); break; }
            name.push_back(char(buffer[i]));
        }
        const auto safe = filenameOnly(name);
        if (safe.empty() || safe.find('.') == std::string::npos) continue;
        if (appendEntry({safe, start, count}, true)) ++valid;
    }
    if (valid < 2 || valid * 100 < rows * 80) {
        fileEntries.clear();
        fileEntries.swap(saved);
        error = "DIR has too many missing or invalid sectors.";
        return false;
    }
    summary = "DIR: " + std::to_string(fileEntries.size()) +
        " files, 2048-byte sectors. Directory names preserved.";
    error.clear();
    return true;
}

bool StorylandImgStream::scanLeedsEntries(std::string& error) {
    const uint64_t count = sourceSize / sectorSize;
    if (count == 0 || count > std::numeric_limits<uint32_t>::max()) return false;
    std::ifstream file(source, std::ios::binary);
    if (!file) {
        error = "Can't read IMG sectors.";
        return false;
    }
    struct Signature {uint32_t sector; std::string extension;};
    std::vector<Signature> starts;
    std::vector<uint8_t> block(scanBlockSectors * sectorSize);
    size_t models = 0, textures = 0;
    for (uint64_t sector = 0; sector < count;) {
        const size_t readSectors = size_t(std::min<uint64_t>(scanBlockSectors, count - sector));
        const size_t readLength = readSectors * size_t(sectorSize);
        file.read(reinterpret_cast<char*>(block.data()), std::streamsize(readLength));
        if (size_t(file.gcount()) != readLength) {
            error = "IMG read ended before the last sector.";
            return false;
        }
        for (size_t index = 0; index < readSectors; ++index) {
            const uint8_t* data = block.data() + index * sectorSize;
            // The raw PC reader supports the retail PS2 MDL/XTX/ANIM/COL2
            // population. Require both model and texture families to label
            // an unindexed file as PS2 Stories; don't invent retail names.
            const auto extension = extensionForSignature(data);
            if (extension.empty()) continue;
            if (extension == ".mdl") ++models;
            if (extension == ".xtx") ++textures;
            starts.push_back({uint32_t(sector + index), extension});
        }
        sector += readSectors;
    }
    if (models < 4 || textures < 4) {
        error = "No GAME.DTZ or confirmed PS2 MDL/XTX sector table was found.";
        return false;
    }
    for (size_t index = 0; index < starts.size(); ++index) {
        const auto& current = starts[index];
        const uint32_t next = index + 1 < starts.size() ? starts[index + 1].sector : uint32_t(count);
        if (next <= current.sector) continue;
        char name[96]{};
        std::snprintf(name,sizeof(name),"sector_%08u%s",current.sector,current.extension.c_str());
        appendEntry({name,current.sector,next - current.sector},false);
    }
    summary = "PS2 Stories IMG: " + std::to_string(fileEntries.size()) +
        " detected files. Select GAME.DTZ for exact names and sizes.";
    error.clear();
    return true;
}

std::string StorylandImgStream::detectExtension(uint64_t offset) const {
    std::vector<uint8_t> header;
    std::string error;
    if (!readAt(offset, 12, header, error)) return "";
    return extensionForSignature(header.data());
}

bool StorylandImgStream::readAt(uint64_t absoluteOffset, size_t length,
                                std::vector<uint8_t>& output, std::string& error) const {
    output.clear();
    if (source.empty() || absoluteOffset > sourceSize ||
        uint64_t(length) > sourceSize - absoluteOffset ||
        absoluteOffset > uint64_t(std::numeric_limits<std::streamoff>::max())) {
        error = "Byte range is outside this IMG.";
        return false;
    }
    std::ifstream file(source, std::ios::binary);
    if (!file || !file.seekg(std::streamoff(absoluteOffset))) {
        error = "Can't seek to this IMG sector.";
        return false;
    }
    output.resize(length);
    if (length && !file.read(reinterpret_cast<char*>(output.data()), std::streamsize(length))) {
        output.clear();
        error = "IMG read was incomplete.";
        return false;
    }
    error.clear();
    return true;
}

bool StorylandImgStream::readEntry(size_t index, size_t maximumBytes,
                                   std::vector<uint8_t>& output, bool& partial,
                                   std::string& error) const {
    partial = false;
    if (index >= fileEntries.size()) {
        error = "Choose an IMG file first.";
        return false;
    }
    const auto& entry = fileEntries[index];
    const size_t amount = size_t(std::min<uint64_t>(entry.byteSize, maximumBytes));
    partial = uint64_t(amount) != entry.byteSize;
    return readAt(entry.byteOffset, amount, output, error);
}

bool StorylandImgStream::exportEntry(size_t index, const std::filesystem::path& destination,
                                     std::string& error) const {
    if (index >= fileEntries.size()) {
        error = "Choose an IMG file first.";
        return false;
    }
    const auto& entry = fileEntries[index];
    if (destination.empty()) {
        error = "Choose an export folder.";
        return false;
    }
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!input || !output || !input.seekg(std::streamoff(entry.byteOffset))) {
        error = "Can't open the IMG or export file.";
        return false;
    }
    std::vector<char> buffer(transferBytes);
    uint64_t remaining = entry.byteSize;
    while (remaining) {
        const size_t amount = size_t(std::min<uint64_t>(remaining, buffer.size()));
        if (!input.read(buffer.data(), std::streamsize(amount)) ||
            !output.write(buffer.data(), std::streamsize(amount))) {
            error = "IMG export was incomplete.";
            output.close();
            std::error_code ignored;
            std::filesystem::remove(destination, ignored);
            return false;
        }
        remaining -= amount;
    }
    output.flush();
    if (!output) {
        error = "Couldn't finish writing this file.";
        output.close();
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
        return false;
    }
    error.clear();
    return true;
}
