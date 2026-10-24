#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct StorylandDataField {
    std::string name;
    std::string value;
    size_t begin = 0;
    size_t length = 0;
    size_t offset = 0;
    bool byteValue = false;
};
struct StorylandDataRow {
    std::string text;
    std::string ending;
    std::string section;
    std::vector<StorylandDataField> fields;
    bool disabled = false;
    bool comment = false;
};
class StorylandDataFile {
public:
    bool loadFromFile(const std::filesystem::path& path, std::string& error);
    bool loadBytes(const std::vector<uint8_t>& bytes, const std::filesystem::path& path, std::string& error);
    bool saveToFile(const std::filesystem::path& path, std::string& error);
    std::vector<uint8_t> bytes() const;
    bool setField(size_t row, size_t field, const std::string& value, std::string& error);
    bool setRow(size_t row, const std::string& text, std::string& error);
    bool insertRow(size_t before, const std::string& text, std::string& error);
    bool removeRow(size_t row, std::string& error);
    bool moveRow(size_t row, int direction, std::string& error);
    bool toggleRow(size_t row, std::string& error);
    std::filesystem::path referencedPath(size_t row) const;
    std::vector<std::string> warnings() const;
    const std::vector<StorylandDataRow>& rows() const { return rows_; }
    const std::filesystem::path& sourcePath() const { return path_; }
    bool dirty() const { return dirty_; }
    bool binaryWater() const { return binaryWater_; }
    bool manifest() const { return manifest_; }
    std::string summary() const;
private:
    void parseRows();
    void updatePathCount();
    std::filesystem::path path_;
    std::vector<StorylandDataRow> rows_;
    std::vector<uint8_t> binary_;
    std::string bom_;
    bool dirty_ = false;
    bool binaryWater_ = false;
    bool manifest_ = false;
    bool countedPath_ = false;
};
