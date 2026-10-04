#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandGxtFormat { Stories, ChinatownWars };
enum class StorylandGxtEncoding { LcsFont, Windows1252, Unicode };
struct StorylandGxtEntry {
    std::string key;
    size_t table = 0;
    std::u16string units;
    size_t offsetField = 0;
    bool edited = false;
};
struct StorylandGxtTable {
    std::string name;
    size_t offset = 0;
    size_t end = 0;
    size_t directoryField = 0;
    size_t dataStart = 0;
    size_t dataSize = 0;
    size_t sizeField = 0;
    std::vector<size_t> entries;
};
class StorylandGxtFile {
public:
    bool loadFromFile(const std::wstring& path, std::string& error);
    bool load(const std::vector<uint8_t>& bytes, std::string& error);
    bool serialize(std::vector<uint8_t>& bytes, std::string& error) const;
    bool saveToFile(const std::wstring& path, std::string& error) const;
    bool setText(size_t entry, const std::u16string& text, std::string& error);
    std::u16string text(size_t entry) const;
    std::string summary() const;
    const std::vector<StorylandGxtEntry>& entries() const { return entries_; }
    const std::vector<StorylandGxtTable>& tables() const { return tables_; }
    const std::wstring& sourcePath() const { return path_; }
    StorylandGxtFormat format() const { return format_; }
    StorylandGxtEncoding encoding() const { return encoding_; }
    void setEncoding(StorylandGxtEncoding value) { encoding_ = value; }
    bool dirty() const;
    const std::string& family() const { return family_; }
private:
    bool parse(std::string& error);
    std::vector<uint8_t> raw_;
    std::vector<StorylandGxtEntry> entries_;
    std::vector<StorylandGxtTable> tables_;
    std::wstring path_;
    std::string family_ = "LCS / VCS";
    StorylandGxtFormat format_ = StorylandGxtFormat::Stories;
    StorylandGxtEncoding encoding_ = StorylandGxtEncoding::Windows1252;
};
