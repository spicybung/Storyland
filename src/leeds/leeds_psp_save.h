#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace storyland_psp_save {
constexpr size_t MaximumMetadataBytes = 256 * 1024;

struct Field {
    uint16_t format = 0;
    std::vector<uint8_t> bytes;
};

struct Metadata {
    std::string title;
    std::string saveTitle;
    std::string detail;
    std::string directory;
    bool protectedData = false;
    bool dataFileListed = false;
    std::array<uint8_t, 16> dataFileHash{};
    std::map<std::string, Field> fields;
};

inline uint32_t word(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::runtime_error("Truncated PSP savedata metadata.");
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
        uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
}

inline std::string stringField(const Metadata& metadata, const char* name) {
    const auto field = metadata.fields.find(name);
    if (field == metadata.fields.end()) return {};
    const auto& bytes = field->second.bytes;
    if (field->second.format != 0x0204 || bytes.empty() || bytes.back() != 0)
        throw std::runtime_error(std::string("Invalid PSP savedata text field: ") + name);
    const auto end = std::find(bytes.begin(), bytes.end(), uint8_t(0));
    return std::string(bytes.begin(), end);
}

inline Metadata decode(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 20 || bytes.size() > MaximumMetadataBytes ||
        word(bytes, 0) != 0x46535000 || word(bytes, 4) != 0x00000101)
        throw std::runtime_error("Unsupported PSP PARAM.SFO header.");
    const size_t keyOffset = word(bytes, 8);
    const size_t dataOffset = word(bytes, 12);
    const size_t count = word(bytes, 16);
    if (count > 256 || 20 + count * 16 > keyOffset ||
        keyOffset > dataOffset || dataOffset > bytes.size())
        throw std::runtime_error("Invalid PSP PARAM.SFO table boundaries.");
    Metadata result;
    size_t decodedBytes = 0;
    for (size_t index = 0; index < count; ++index) {
        const size_t offset = 20 + index * 16;
        const uint32_t keyAndFormat = word(bytes, offset);
        const size_t key = keyAndFormat & 0xffff;
        const uint16_t format = uint16_t(keyAndFormat >> 16);
        const size_t length = word(bytes, offset + 4);
        const size_t capacity = word(bytes, offset + 8);
        const size_t data = word(bytes, offset + 12);
        if (key >= dataOffset - keyOffset || length > capacity ||
            data > bytes.size() - dataOffset || capacity > bytes.size() - dataOffset - data ||
            length > MaximumMetadataBytes - decodedBytes ||
            (format != 0x0004 && format != 0x0204 && format != 0x0404) ||
            (format == 0x0404 && length != 4))
            throw std::runtime_error("Invalid PSP PARAM.SFO field.");
        decodedBytes += length;
        const auto keyStart = bytes.begin() + keyOffset + key;
        const auto keyEnd = std::find(keyStart, bytes.begin() + dataOffset, uint8_t(0));
        if (keyEnd == keyStart || keyEnd == bytes.begin() + dataOffset ||
            keyEnd - keyStart > 127)
            throw std::runtime_error("Invalid PSP PARAM.SFO key.");
        const std::string name(keyStart, keyEnd);
        Field field;
        field.format = format;
        field.bytes.assign(bytes.begin() + dataOffset + data,
            bytes.begin() + dataOffset + data + length);
        if (!result.fields.emplace(name, std::move(field)).second)
            throw std::runtime_error("Duplicate PSP PARAM.SFO key.");
    }
    if (stringField(result, "CATEGORY") != "MS")
        throw std::runtime_error("The PARAM.SFO file does not describe PSP savedata.");
    result.title = stringField(result, "TITLE");
    result.saveTitle = stringField(result, "SAVEDATA_TITLE");
    result.detail = stringField(result, "SAVEDATA_DETAIL");
    result.directory = stringField(result, "SAVEDATA_DIRECTORY");
    if (result.title.empty() || result.directory.empty())
        throw std::runtime_error("PSP savedata title or directory is missing.");
    const auto parameters = result.fields.find("SAVEDATA_PARAMS");
    if (parameters != result.fields.end()) {
        if (parameters->second.format != 0x0004 || parameters->second.bytes.size() != 128)
            throw std::runtime_error("Invalid PSP savedata protection metadata.");
        result.protectedData = parameters->second.bytes[0] != 0;
    }
    const auto files = result.fields.find("SAVEDATA_FILE_LIST");
    if (files != result.fields.end()) {
        const auto& entries = files->second.bytes;
        if (files->second.format != 0x0004 || entries.size() % 32 != 0)
            throw std::runtime_error("Invalid PSP savedata file list.");
        for (size_t offset = 0; offset < entries.size(); offset += 32) {
            const auto start = entries.begin() + offset;
            const auto end = std::find(start, start + 13, uint8_t(0));
            if (end == start + 13)
                throw std::runtime_error("Invalid PSP savedata filename.");
            if (std::string(start, end) == "DATA.BIN") {
                if (result.dataFileListed)
                    throw std::runtime_error("Duplicate PSP savedata DATA.BIN entry.");
                result.dataFileListed = true;
                std::copy_n(start + 13, result.dataFileHash.size(), result.dataFileHash.begin());
            }
        }
    }
    return result;
}

inline Metadata readMetadata(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto length = input.tellg();
    if (!input || length < 20 || uint64_t(length) > MaximumMetadataBytes)
        throw std::runtime_error("Cannot read PSP PARAM.SFO, or its size is invalid.");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size())))
        throw std::runtime_error("Cannot read the complete PSP PARAM.SFO.");
    return decode(bytes);
}

inline std::string upperFilename(const std::filesystem::path& path) {
    std::string name = path.filename().u8string();
    for (char& letter : name) if (letter >= 'a' && letter <= 'z') letter -= 'a' - 'A';
    return name;
}

inline std::filesystem::path metadataPath(const std::filesystem::path& selected) {
    const auto name = upperFilename(selected);
    if (name == "PARAM.SFO") return selected;
    if (name != "DATA.BIN") return {};
    std::error_code error;
    for (const auto* filename : {"PARAM.SFO", "param.sfo"}) {
        const auto candidate = selected.parent_path() / filename;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
        error.clear();
    }
    return {};
}

inline bool describeUnsupportedImport(const std::filesystem::path& selected, std::string& description) {
    const auto path = metadataPath(selected);
    if (path.empty()) return false;
    try {
        const auto metadata = readMetadata(path);
        description = metadata.title + "\n" + metadata.saveTitle + "\n" + metadata.detail +
            "\nPSP savedata: " + metadata.directory + ". ";
        if (metadata.protectedData)
            description += "Protected DATA.BIN decryption and PSP gameplay import are not implemented. ";
        else description += "PSP gameplay import is not implemented. ";
        description += "The displayed details are savedata metadata; no game state was loaded.";
    } catch (const std::exception& error) {
        description = std::string("Cannot inspect PSP savedata: ") + error.what();
    }
    return true;
}
}
