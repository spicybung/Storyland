#pragma once
#include "src/leeds/leeds_anim.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

class StorylandMocapArchive {
public:
    struct Entry {
        std::string name;
        uint64_t offset=0;
        uint64_t length=0;
    };

    bool load(const std::filesystem::path& directoryFile,
        const std::filesystem::path& imageFile, std::string& error) {
        entries.clear();
        imagePath.clear();
        std::ifstream directory(directoryFile, std::ios::binary | std::ios::ate);
        if (!directory) {
            error = "Native CUTS/MOCAP .DIR file is missing.";
            return false;
        }
        const auto directoryBytes = directory.tellg();
        if (directoryBytes <= 0 || directoryBytes > std::streamoff(4096 * 32) ||
            directoryBytes % std::streamoff(32) != 0) {
            error = "Native CUTS/MOCAP .DIR has an invalid directory length.";
            return false;
        }
        std::vector<uint8_t> rows(static_cast<size_t>(directoryBytes));
        directory.seekg(0, std::ios::beg);
        directory.read(reinterpret_cast<char*>(rows.data()), std::streamsize(rows.size()));
        if (!directory) {
            error = "Native CUTS/MOCAP .DIR is truncated.";
            return false;
        }
        return loadDirectoryRows(rows.data(), rows.size() / 32, imageFile, error);
    }

    bool loadLcsEmbeddedDirectory(const std::vector<uint8_t>& game,
        const std::filesystem::path& imageFile, std::string& error) {
        entries.clear();
        imagePath.clear();
        const auto word = [&](size_t offset) -> uint32_t {
            return uint32_t(game[offset]) | uint32_t(game[offset + 1]) << 8 |
                uint32_t(game[offset + 2]) << 16 | uint32_t(game[offset + 3]) << 24;
        };
        if (game.size() < 0xC8 || game[0] != 'G' || game[1] != 'A' ||
            game[2] != 'T' || game[3] != 'G') {
            error = "The embedded LCS CUTS directory requires an unpacked GATG archive.";
            return false;
        }
        const uint32_t descriptor = word(0xC4);
        if (!descriptor || uint64_t(descriptor) + 12 > game.size()) {
            error = "The embedded LCS CUTS directory descriptor is outside GAME.DTZ.";
            return false;
        }
        const uint32_t table = word(descriptor);
        const uint32_t capacity = word(size_t(descriptor) + 4);
        const uint32_t count = word(size_t(descriptor) + 8);
        if (!table || !count || count > capacity || capacity > 4096 ||
            uint64_t(table) + uint64_t(count) * 32 > game.size()) {
            error = "The embedded LCS CUTS directory table or entry count is invalid.";
            return false;
        }
        return loadDirectoryRows(game.data() + table, count, imageFile, error);
    }

    bool extract(const std::string& file,std::vector<uint8_t>& bytes,std::string& error) const {
        bytes.clear();
        std::string wanted=file;
        std::transform(wanted.begin(),wanted.end(),wanted.begin(),[](unsigned char ch){return char(std::toupper(ch));});
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& value){return value.name==wanted;});
        if(found==entries.end()) {error="Cutscene resource was not found: "+file;return false;}
        if(found->length==0) {error="Cutscene resource has no sector payload: "+file;return false;}
        std::ifstream image(imagePath,std::ios::binary);
        if(!image) {error="CUTS/MOCAP .IMG cannot be opened.";return false;}
        image.seekg(std::streamoff(found->offset),std::ios::beg);
        bytes.resize(size_t(found->length));
        image.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
        if(!image) {error="CUTS/MOCAP .IMG resource is truncated.";bytes.clear();return false;}
        error.clear();return true;
    }

    const std::vector<Entry>& files() const {return entries;}
    bool available() const {return !entries.empty();}
    bool contains(const std::string& filename) const {
        std::string requested=filename;
        std::transform(requested.begin(),requested.end(),requested.begin(),[](unsigned char character) {
            return char(std::toupper(character));
        });
        return std::any_of(entries.begin(),entries.end(),[&](const Entry& entry) {
            return entry.name==requested&&entry.length!=0;
        });
    }
    bool hasScene(const std::string& name) const {
        if(!contains(name+".CUT"))return false;
        const bool nativeScene=contains(name+".CAM")&&contains(name+".ANIM");
        const bool classicScene=contains(name+".DAT")&&contains(name+".IFP");
        return nativeScene||classicScene;
    }

    bool findAnimation(const std::string& requested,StorylandAnimFile& animation,
        std::string& matchedFile,std::string& error) const {
        matchedFile.clear();
        if(requested.empty()||requested.size()>64) {
            error="Cutscene animation lookup requires a valid animation or bank name.";
            return false;
        }
        const auto normalize=[](std::string value) {
            std::string normalized;
            for(unsigned char character:value)if(std::isalnum(character))
                normalized.push_back(char(std::toupper(character)));
            return normalized;
        };
        const std::string target=normalize(requested);
        const auto parse=[&](const Entry& entry,StorylandAnimFile& result) {
            std::vector<uint8_t> bytes;
            std::string localError;
            return extract(entry.name,bytes,localError)&&
                result.loadFromMemory(bytes,std::wstring(entry.name.begin(),entry.name.end()),localError)&&
                result.hasDecodedMotion()&&!result.clips().empty();
        };
        // First resolve direct resource names. A filename may name a bank while
        // the clips inside it have names such as csBry, cslanSt and csplr.
        for(const auto& entry:entries) {
            const auto stem=std::filesystem::path(entry.name).stem().string();
            if(normalize(stem)!=target||
                std::filesystem::path(entry.name).extension()!=".ANIM")continue;
            StorylandAnimFile candidate;
            if(parse(entry,candidate)) {
                animation=std::move(candidate);matchedFile=entry.name;error.clear();return true;
            }
        }
        // MOCAP .ANIM banks and individual clip identifiers are not one-to-one.
        // Search the verified clip headers in the retail sector archive, without
        // guessing a filename from the cutscene's script label.
        for(const auto& entry:entries) {
            if(std::filesystem::path(entry.name).extension()!=".ANIM")continue;
            StorylandAnimFile candidate;
            if(!parse(entry,candidate))continue;
            for(size_t index=0;index<candidate.clips().size();++index) {
                const auto& clip=candidate.clips()[index];
                if(normalize(clip.name)==target||normalize(clip.bankName)==target) {
                    candidate.setActiveClipIndex(uint32_t(index));
                    animation=std::move(candidate);matchedFile=entry.name;error.clear();return true;
                }
            }
        }
        error="No decodable retail MOCAP .ANIM resource or clip matches '"+requested+"'.";
        return false;
    }

private:
    bool loadDirectoryRows(const uint8_t* rows, size_t count,
        const std::filesystem::path& imageFile, std::string& error) {
        std::ifstream image(imageFile, std::ios::binary | std::ios::ate);
        if (!image) {
            error = "Native CUTS/MOCAP .IMG file is missing.";
            return false;
        }
        const auto imageSize = image.tellg();
        if (imageSize < 32 || imageSize > std::streamoff(512ull * 1024 * 1024)) {
            error = "CUTS/MOCAP .IMG has an invalid size.";
            return false;
        }
        std::vector<Entry> decoded;
        for (size_t index = 0; index < count; ++index) {
            const uint8_t* row = rows + index * 32;
            const auto word = [&](size_t offset) -> uint32_t {
                return uint32_t(row[offset]) | uint32_t(row[offset + 1]) << 8 |
                    uint32_t(row[offset + 2]) << 16 | uint32_t(row[offset + 3]) << 24;
            };
            Entry entry;
            for (size_t offset = 8; offset < 32 && row[offset]; ++offset) {
                const unsigned char character = row[offset];
                if (!std::isalnum(character) && character != '_' && character != '.' && character != '-') {
                    error = "CUTS/MOCAP directory has an invalid resource name.";
                    return false;
                }
                entry.name.push_back(char(std::toupper(character)));
            }
            if (entry.name.empty()) continue;
            entry.offset = uint64_t(word(0)) * 2048ull;
            entry.length = uint64_t(word(4)) * 2048ull;
            if (entry.offset + entry.length > uint64_t(imageSize) ||
                entry.length > 64ull * 1024 * 1024) {
                error = "CUTS/MOCAP directory references an invalid .IMG sector range.";
                return false;
            }
            if (std::any_of(decoded.begin(), decoded.end(), [&](const Entry& previous) {
                    return previous.name == entry.name;
                })) {
                error = "CUTS/MOCAP directory repeats a resource name.";
                return false;
            }
            decoded.push_back(std::move(entry));
        }
        if (decoded.empty()) {
            error = "CUTS/MOCAP directory has no valid resources.";
            return false;
        }
        entries = std::move(decoded);
        imagePath = imageFile;
        error.clear();
        return true;
    }

    std::filesystem::path imagePath;
    std::vector<Entry> entries;
};
