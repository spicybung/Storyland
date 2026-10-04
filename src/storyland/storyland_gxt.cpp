#include "storyland_gxt.h"
#include "storyland_atomic_io.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace {
uint16_t read16(const std::vector<uint8_t>& bytes, size_t position) {
    return uint16_t(bytes[position]) | uint16_t(bytes[position + 1]) << 8;
}
uint32_t read32(const std::vector<uint8_t>& bytes, size_t position) {
    return uint32_t(read16(bytes, position)) | uint32_t(read16(bytes, position + 2)) << 16;
}
void write32(std::vector<uint8_t>& bytes, size_t position, uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) bytes[position + index] = uint8_t(value >> (8 * index));
}
void append16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(uint8_t(value));
    bytes.push_back(uint8_t(value >> 8));
}
bool hasTag(const std::vector<uint8_t>& bytes, size_t position, const char* tag) {
    return position <= bytes.size() && bytes.size() - position >= 4 && !std::memcmp(bytes.data() + position, tag, 4);
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string readName(const std::vector<uint8_t>& bytes, size_t position) {
    std::string result;
    for (size_t index = 0; index < 8 && bytes[position + index]; ++index) {
        require(bytes[position + index] >= 32 && bytes[position + index] <= 126, "Invalid GXT table/key name.");
        result.push_back(char(bytes[position + index]));
    }
    return result;
}
const uint16_t windows1252[32] = {
    0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,
    0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,
    0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
    0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178
};
const uint16_t lcsTail[32] = {
    0x20AC,0x201A,0x201E,0x2C6,0x2039,0x152,0x2018,0x2019,
    0x201C,0x201D,0x2DC,0x2122,0x203A,0x153,0x178,0xA1,
    0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,
    0xAA,0xAB,0xAE,0xB0,0xB9,0xBB,0xBF,0x7F
};
uint16_t decode(uint16_t value, StorylandGxtEncoding encoding) {
    if (encoding == StorylandGxtEncoding::LcsFont) {
        if (value >= 0x80 && value <= 0xBF) return uint16_t(value + 0x40);
        if (value >= 0xC0 && value <= 0xDF) return lcsTail[value - 0xC0];
    } else if (encoding == StorylandGxtEncoding::Windows1252 && value >= 0x80 && value <= 0x9F) {
        return windows1252[value - 0x80];
    }
    return value;
}
void appendEscape(std::u16string& result, uint16_t value) {
    const char16_t* digits = u"0123456789ABCDEF";
    result += u"~#";
    for (int shift = 12; shift >= 0; shift -= 4) result += digits[(value >> shift) & 15];
    result += u"~";
}
}
bool StorylandGxtFile::loadFromFile(const std::wstring& path, std::string& error) {
    std::ifstream input(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!input) { error = "Could not open GXT file."; return false; }
    const auto size = input.tellg();
    if (size < 0 || size > 64 * 1024 * 1024) { error = "GXT exceeds the 64 MiB size limit."; return false; }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()))) {
        error = "Could not read GXT file."; return false;
    }
    if (!load(bytes, error)) return false;
    path_ = path;
    return true;
}
bool StorylandGxtFile::load(const std::vector<uint8_t>& bytes, std::string& error) {
    StorylandGxtFile candidate;
    candidate.raw_ = bytes;
    if (!candidate.parse(error)) return false;
    *this = std::move(candidate);
    return true;
}
bool StorylandGxtFile::parse(std::string& error) {
    try {
        require(raw_.size() >= 8 && raw_.size() <= 64 * 1024 * 1024, "Invalid GXT size.");
        size_t decodedUnits = 0;
        if (!std::memcmp(raw_.data(), "DS_GXT", 6)) {
            format_ = StorylandGxtFormat::ChinatownWars;
            family_ = "Chinatown Wars";
            encoding_ = StorylandGxtEncoding::Unicode;
            StorylandGxtTable table;
            table.name = "Strings";
            table.offset = 8;
            table.end = raw_.size();
            size_t position = 8;
            const unsigned count = read16(raw_, 6);
            for (unsigned index = 0; index < count; ++index) {
                require(position <= raw_.size() && raw_.size() - position >= 2, "Truncated DS_GXT string length.");
                const unsigned length = read16(raw_, position);
                position += 2;
                require(length <= (raw_.size() - position) / 2, "Truncated DS_GXT string data.");
                StorylandGxtEntry entry;
                entry.key = std::to_string(index);
                for (unsigned unit = 0; unit < length; ++unit) entry.units.push_back(char16_t(read16(raw_, position + 2 * unit)));
                position += 2 * length;
                table.entries.push_back(entries_.size());
                entries_.push_back(std::move(entry));
            }
            table.dataStart = position;
            tables_.push_back(std::move(table));
        } else {
            require(hasTag(raw_, 0, "TABL"), "Unsupported GXT signature: expected TABL or DS_GXT.");
            const size_t directorySize = read32(raw_, 4);
            require(directorySize > 0 && directorySize % 12 == 0 && directorySize <= raw_.size() - 8, "Invalid TABL directory size.");
            for (size_t position = 8; position < 8 + directorySize; position += 12) {
                StorylandGxtTable table;
                table.name = readName(raw_, position);
                table.offset = read32(raw_, position + 8);
                table.directoryField = position + 8;
                require(!table.name.empty() && table.offset >= 8 + directorySize && table.offset < raw_.size(), "Invalid GXT table offset.");
                tables_.push_back(std::move(table));
            }
            std::vector<size_t> order(tables_.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](size_t first, size_t second) { return tables_[first].offset < tables_[second].offset; });
            for (size_t index = 0; index < order.size(); ++index) {
                auto& table = tables_[order[index]];
                table.end = index + 1 < order.size() ? tables_[order[index + 1]].offset : raw_.size();
                require(table.end > table.offset, "Duplicate or overlapping GXT table offsets.");
                size_t position = table.offset;
                if (table.name != "MAIN") {
                    require(table.end - position >= 8 && readName(raw_, position) == table.name, "GXT table prefix does not match TABL.");
                    position += 8;
                }
                require(table.end - position >= 8 && hasTag(raw_, position, "TKEY"), "Missing TKEY block.");
                const size_t keyBytes = read32(raw_, position + 4);
                position += 8;
                require(keyBytes % 12 == 0 && keyBytes <= table.end - position, "Invalid TKEY size.");
                const size_t keyStart = position;
                position += keyBytes;
                require(table.end - position >= 8 && hasTag(raw_, position, "TDAT"), "Missing TDAT block.");
                table.sizeField = position + 4;
                table.dataSize = read32(raw_, position + 4);
                table.dataStart = position + 8;
                require(table.dataSize % 2 == 0 && table.dataSize <= table.end - table.dataStart, "Invalid TDAT size.");
                for (size_t key = keyStart; key < keyStart + keyBytes; key += 12) {
                    const size_t offset = read32(raw_, key);
                    require(offset % 2 == 0 && offset < table.dataSize, "TKEY string offset outside TDAT.");
                    StorylandGxtEntry entry;
                    entry.key = readName(raw_, key + 4);
                    entry.table = order[index];
                    entry.offsetField = key;
                    size_t stringPosition = table.dataStart + offset;
                    const size_t end = table.dataStart + table.dataSize;
                    for (; stringPosition < end && read16(raw_, stringPosition); stringPosition += 2) {
                        require(++decodedUnits <= 32 * 1024 * 1024, "Decoded GXT text exceeds the 32M code unit limit.");
                        entry.units.push_back(char16_t(read16(raw_, stringPosition)));
                    }
                    require(stringPosition < end, "Unterminated TDAT string.");
                    table.entries.push_back(entries_.size());
                    entries_.push_back(std::move(entry));
                }
            }
            for (const auto& table : tables_) {
                if (table.name == "SAL1") { family_ = "LCS"; encoding_ = StorylandGxtEncoding::LcsFont; break; }
                if (table.name == "CHCOL1") { family_ = "VCS"; encoding_ = StorylandGxtEncoding::Windows1252; break; }
            }
        }
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
bool StorylandGxtFile::dirty() const {
    for (const auto& entry : entries_) if (entry.edited) return true;
    return false;
}
std::string StorylandGxtFile::summary() const {
    return family_ + " GXT | " + std::to_string(tables_.size()) + " tables | " + std::to_string(entries_.size()) + " strings" + (dirty() ? " | modified" : "");
}
std::u16string StorylandGxtFile::text(size_t index) const {
    std::u16string result;
    if (index >= entries_.size()) return result;
    const auto& units = entries_[index].units;
    const size_t count = units.size() - (format_ == StorylandGxtFormat::ChinatownWars && index + 1 == entries_.size() && !units.empty() && units.back() == 0 ? 1 : 0);
    for (size_t position = 0; position < count; ++position) {
        const uint16_t value = units[position];
        if (value == u'~' && position + 1 < units.size() && units[position + 1] == u'#') {
            appendEscape(result, value);
            continue;
        }
        const uint16_t displayed = decode(value, encoding_);
        const bool outsideFont = encoding_ != StorylandGxtEncoding::Unicode && (value > 255 || (encoding_ == StorylandGxtEncoding::LcsFont && value >= 0xE0));
        if (outsideFont || displayed == 0x7F || (displayed >= 0x80 && displayed <= 0x9F) || value == 0 ||
            (value < 32 && value != 10 && value != 9) || value >= 0xFEEF || (value >= 0xD800 && value <= 0xDFFF)) {
            appendEscape(result, value);
        } else result += char16_t(displayed);
    }
    return result;
}
bool StorylandGxtFile::setText(size_t index, const std::u16string& text, std::string& error) {
    if (index >= entries_.size()) { error = "No GXT string selected."; return false; }
    std::u16string units;
    for (size_t position = 0; position < text.size(); ++position) {
        uint16_t value = text[position];
        if (value == u'~' && position + 1 < text.size() && text[position + 1] == u'#') {
            if (position + 6 >= text.size() || text[position + 6] != u'~') { error = "Raw codes must use ~#XXXX~ (four hexadecimal digits)."; return false; }
            value = 0;
            for (size_t digit = position + 2; digit < position + 6; ++digit) {
                unsigned hex = text[digit];
                if (hex >= '0' && hex <= '9') hex -= '0';
                else if (hex >= 'A' && hex <= 'F') hex = hex - 'A' + 10;
                else if (hex >= 'a' && hex <= 'f') hex = hex - 'a' + 10;
                else { error = "Invalid raw GXT code."; return false; }
                value = uint16_t((value << 4) | hex);
            }
            position += 6;
        } else {
            if (value == 13 && position + 1 < text.size() && text[position + 1] == 10) continue;
            if (encoding_ != StorylandGxtEncoding::Unicode) {
                bool found = false;
                for (unsigned raw = 1; raw < 256; ++raw) {
                    if (decode(uint16_t(raw), encoding_) == value) { value = uint16_t(raw); found = true; break; }
                }
                if (!found) { error = "Character unavailable in this font. Choose Unicode or use ~#XXXX~ for a raw code."; return false; }
            }
        }
        if (value == 0 && format_ == StorylandGxtFormat::Stories) { error = "Stories strings cannot contain embedded null codes."; return false; }
        units += char16_t(value);
    }
    if (format_ == StorylandGxtFormat::ChinatownWars && index + 1 == entries_.size() && !entries_[index].units.empty() &&
        entries_[index].units.back() == 0 && (units.empty() || units.back() != 0)) units.push_back(0);
    if (format_ == StorylandGxtFormat::ChinatownWars && units.size() > 65535) { error = "DS_GXT string exceeds 65535 code units."; return false; }
    auto& entry = entries_[index];
    if (entry.units != units) { entry.units = std::move(units); entry.edited = true; }
    error.clear();
    return true;
}
bool StorylandGxtFile::serialize(std::vector<uint8_t>& bytes, std::string& error) const {
    bytes.clear();
    if (raw_.empty()) { error = "No GXT loaded."; return false; }
    if (!dirty()) { bytes = raw_; error.clear(); return true; }
    if (format_ == StorylandGxtFormat::ChinatownWars) {
        bytes.insert(bytes.end(), raw_.begin(), raw_.begin() + 8);
        for (const auto& entry : entries_) {
            append16(bytes, uint16_t(entry.units.size()));
            for (char16_t value : entry.units) append16(bytes, value);
        }
        bytes.insert(bytes.end(), raw_.begin() + tables_[0].dataStart, raw_.end());
    } else {
        std::vector<size_t> order(tables_.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](size_t first, size_t second) { return tables_[first].offset < tables_[second].offset; });
        bytes.insert(bytes.end(), raw_.begin(), raw_.begin() + tables_[order[0]].offset);
        for (size_t index : order) {
            const auto& table = tables_[index];
            const size_t base = bytes.size();
            if (base > UINT32_MAX) { error = "GXT offsets exceed 32 bits."; return false; }
            write32(bytes, table.directoryField, uint32_t(base));
            std::vector<uint8_t> block(raw_.begin() + table.offset, raw_.begin() + table.end);
            std::vector<uint8_t> added;
            for (size_t id : table.entries) {
                const auto& entry = entries_[id];
                if (!entry.edited) continue;
                const size_t offset = table.dataSize + added.size();
                if (offset > UINT32_MAX) { error = "TDAT exceeds 32 bits."; return false; }
                write32(block, entry.offsetField - table.offset, uint32_t(offset));
                for (char16_t value : entry.units) append16(added, value);
                append16(added, 0);
            }
            const size_t newSize = table.dataSize + added.size();
            if (newSize > UINT32_MAX) { error = "TDAT exceeds 32 bits."; return false; }
            write32(block, table.sizeField - table.offset, uint32_t(newSize));
            block.insert(block.begin() + (table.dataStart + table.dataSize - table.offset), added.begin(), added.end());
            bytes.insert(bytes.end(), block.begin(), block.end());
        }
    }
    StorylandGxtFile check;
    if (!check.load(bytes, error)) { error = "Serialized GXT validation failed: " + error; return false; }
    if (check.entries_.size() != entries_.size()) { error = "Serialized GXT entry count changed."; return false; }
    for (size_t index = 0; index < entries_.size(); ++index) {
        if (check.entries_[index].units != entries_[index].units || check.entries_[index].key != entries_[index].key) {
            error = "Serialized GXT text/key mismatch."; return false;
        }
    }
    error.clear();
    return true;
}
bool StorylandGxtFile::saveToFile(const std::wstring& path, std::string& error) const {
    std::vector<uint8_t> bytes;
    if (!serialize(bytes, error)) return false;
    return storylandWriteFilesTransaction({{std::filesystem::path(path), &bytes}}, error);
}
