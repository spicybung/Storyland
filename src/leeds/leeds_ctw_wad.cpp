#include "src/leeds/leeds_ctw_wad.h"
#include "src/leeds/leeds_ctw_pak.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>

namespace storyland_ctw {

void WadArchive::clear() {
    wadPath_.clear();
    tocPath_.clear();
    resources_.clear();
    fileSize_ = 0;
}

bool WadArchive::loadFromFile(const std::filesystem::path& path, std::string& error) {
    clear();
    std::filesystem::path toc = path;
    std::filesystem::path wad = path;
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return char(std::tolower(character)); });
    const bool openingToc = extension == ".toc";
    const auto chooseCompanion = [](const std::filesystem::path& original,
                                    const char* upper, const char* lower) {
        std::filesystem::path result = original;
        result.replace_extension(upper);
        std::error_code error;
        if (std::filesystem::exists(result, error)) return result;
        result.replace_extension(lower);
        return result;
    };
    if (openingToc) wad = chooseCompanion(path, ".WAD", ".wad");
    else toc = chooseCompanion(path, ".TOC", ".toc");

    std::ifstream input(toc, std::ios::binary | std::ios::ate);
    std::ifstream data(wad, std::ios::binary | std::ios::ate);
    if (!input || !data) {
        error = "CTW ROM.WAD and ROM.TOC must be in the same folder.";
        return false;
    }
    const std::streamoff tocLength = input.tellg();
    const std::streamoff wadLength = data.tellg();
    if (tocLength <= 0 || tocLength % 12 != 0 || wadLength <= 0 ||
        tocLength / 12 > 200000) {
        error = "Invalid ROM.TOC length or ROM.WAD file size.";
        return false;
    }
    const size_t count = size_t(tocLength / 12);
    std::vector<WadResource> entries;
    entries.reserve(count);
    input.seekg(0);
    data.seekg(0);
    std::array<unsigned char, 12> record{};
    std::array<unsigned char, 16> beginning{};
    uint32_t previousHash = 0;

    for (size_t i = 0; i < count; ++i) {
        if (!input.read(reinterpret_cast<char*>(record.data()), std::streamsize(record.size()))) {
            error = "ROM.TOC is truncated.";
            return false;
        }
        WadResource entry;
        entry.hash = read32(record.data());
        entry.offset = read32(record.data() + 4);
        entry.size = read32(record.data() + 8);
        if (entry.size == 0 || uint64_t(entry.offset) >= uint64_t(wadLength) ||
            uint64_t(entry.size) > uint64_t(wadLength) - entry.offset ||
            entry.offset % 512u != 0 || (i > 0 && entry.hash <= previousHash)) {
            error = "ROM.TOC contains a corrupt hash, sector offset, or resource size.";
            return false;
        }
        previousHash = entry.hash;
        data.seekg(entry.offset);
        beginning.fill(0);
        if (!data.read(reinterpret_cast<char*>(beginning.data()),
                       std::streamsize(std::min<size_t>(beginning.size(), entry.size)))) {
            error = "Cannot read ROM.WAD resource header.";
            return false;
        }
        if (entry.size >= 6u && beginning[0] == 'D' && beginning[1] == 'S' &&
            beginning[2] == '_' && beginning[3] == 'G' && beginning[4] == 'X' && beginning[5] == 'T')
            entry.fileType = "GXT";
        else if (entry.size >= 4u && beginning[0] == 0x7F && beginning[1] == 'E' &&
                 beginning[2] == 'L' && beginning[3] == 'F') entry.fileType = "ELF";
        else if (entry.hash == 0x6252DA25u && entry.size == 42240u) entry.fileType = "timecycle.dat (PSP)";
        else entry.fileType = "Binary";
        entries.push_back(std::move(entry));
    }

    std::vector<size_t> byOffset(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) byOffset[i] = i;
    std::sort(byOffset.begin(), byOffset.end(), [&](size_t a, size_t b) {
        return entries[a].offset < entries[b].offset;
    });
    for (size_t i = 1; i < byOffset.size(); ++i) {
        const auto& previous = entries[byOffset[i - 1u]];
        const auto& current = entries[byOffset[i]];
        if (uint64_t(previous.offset) + previous.size > current.offset) {
            error = "ROM.TOC contains overlapping resource ranges.";
            return false;
        }
    }

    tocPath_ = toc;
    wadPath_ = wad;
    fileSize_ = uint64_t(wadLength);
    resources_ = std::move(entries);
    error.clear();
    return true;
}

bool WadArchive::extractResource(size_t index, std::vector<uint8_t>& bytes, std::string& error) const {
    bytes.clear();
    if (index >= resources_.size() || wadPath_.empty()) {
        error = "Invalid CTW ROM.WAD resource index.";
        return false;
    }
    const auto& resource = resources_[index];
    std::ifstream data(wadPath_, std::ios::binary);
    if (!data || !data.seekg(resource.offset)) {
        error = "Cannot seek to CTW ROM.WAD resource.";
        return false;
    }
    bytes.resize(resource.size);
    if (!data.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()))) {
        bytes.clear();
        error = "CTW ROM.WAD resource data is truncated.";
        return false;
    }
    error.clear();
    return true;
}

bool WadArchive::exportResource(size_t index, const std::filesystem::path& path, std::string& error) const {
    if (isArchiveSource(wadPath_, path) || isArchiveSource(tocPath_, path)) {
        error = "Cannot export a resource over its source archive or index.";
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!extractResource(index, bytes, error)) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output || !output.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()))) {
        error = "Could not write extracted CTW ROM.WAD resource.";
        return false;
    }
    output.close();
    if (!output) {
        error = "Could not finish writing extracted CTW ROM.WAD resource.";
        return false;
    }
    error.clear();
    return true;
}

}
