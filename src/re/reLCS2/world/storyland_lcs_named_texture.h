#pragma once
#include "src/leeds/leeds_dtz.h"
#include "src/leeds/leeds_native_texture.h"
#include <algorithm>
#include <cctype>
#include <set>

// Resolve the actual dictionary/raster pair carried by wide LCS world
// resources. Identical raster names in unrelated dictionaries are not aliases.
inline bool storylandResolveLcsNamedWorldTexture(const StorylandDtzArchive& archive,
    std::string qualified,RgbaImage& image,std::string& error) {
    image={};
    std::transform(qualified.begin(),qualified.end(),qualified.begin(),[](unsigned char c) {
        return c=='\\'?'/':char(std::tolower(c));
    });
    const size_t slash=qualified.find('/');
    if(slash==std::string::npos||slash==0||slash+1==qualified.size()||qualified.find('/',slash+1)!=std::string::npos) {
        error="Native texture has no exact dictionary/raster pair: "+qualified;return false;
    }
    const std::string dictionaryName=qualified.substr(0,slash),rasterName=qualified.substr(slash+1);
    const auto stem=[](std::string name) {
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
        const auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name.erase(0,slash+1);
        const auto dot=name.find_last_of('.');if(dot!=std::string::npos)name.resize(dot);
        return name;
    };
    constexpr size_t maximumBytes=16u*1024u*1024u;
    bool ambiguous=false;
    const auto accept=[&](RgbaImage candidate) {
        if(candidate.width<=0||candidate.height<=0||candidate.width>2048||candidate.height>2048||
            candidate.rgba.size()!=size_t(candidate.width)*size_t(candidate.height)*4u||candidate.rgba.size()>maximumBytes)return;
        if(image.rgba.empty())image=std::move(candidate);
        else if(image.width!=candidate.width||image.height!=candidate.height||image.rgba!=candidate.rgba)ambiguous=true;
    };
    for(size_t index=0;index<archive.dirEntries().size();++index) {
        const auto& entry=archive.dirEntries()[index];
        if(stem(entry.name)!=dictionaryName||entry.byteLength>maximumBytes||
            (entry.detectedExtension!=".chk"&&entry.detectedExtension!=".xtx"&&entry.detectedExtension!=".txd"))continue;
        std::vector<uint8_t> bytes;LeedsTextureArchive textures;std::string diagnostic;
        if(!archive.extractDirEntryBytes(index,bytes,diagnostic)||
            !textures.loadFromMemory(bytes,LeedsPlatform::Auto,diagnostic))continue;
        for(size_t raster=0;raster<textures.textures().size();++raster) {
            const auto& source=textures.textures()[raster];
            if(stem(source.name)!=rasterName||source.width<=0||source.height<=0||source.width>2048||source.height>2048||
                uint64_t(source.width)*uint64_t(source.height)*4u>maximumBytes)continue;
            RgbaImage candidate;if(textures.decodeTexture(raster,candidate,diagnostic))accept(std::move(candidate));
        }
    }
    if(ambiguous) {image={};error="Ambiguous native texture dictionary: "+qualified;return false;}
    if(!image.rgba.empty()) {error.clear();return true;}
    const auto& bytes=archive.unpackedBytes();
    const auto word=[&](size_t at)->uint32_t {
        if(at>bytes.size()||bytes.size()-at<4)return 0;
        return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;
    };
    const uint32_t table=word(0x60),rows=word(table),count=word(size_t(table)+8);
    if(table&&rows&&count<=4096&&uint64_t(rows)+uint64_t(count)*28<=bytes.size()) {
        const std::set<std::string> wanted{rasterName};
        for(uint32_t index=0;index<count;++index) {
            const size_t row=size_t(rows)+size_t(index)*28;std::string name;
            for(size_t letter=0;letter<20&&bytes[row+8+letter];++letter) {
                const uint8_t c=bytes[row+8+letter];if(c<32||c>126) {name.clear();break;}name.push_back(char(c));
            }
            if(stem(name)!=dictionaryName)continue;
            const uint32_t dictionary=word(row);if(!dictionary)continue;
            for(auto& texture:storylandDecodeNativeTextureDictionary(bytes,dictionary,
                archive.gameVariant()==StorylandDtzGameVariant::Vcs,&wanted,maximumBytes))
                if(stem(texture.name)==rasterName)accept(std::move(texture.image));
        }
    }
    if(ambiguous) {image={};error="Ambiguous native texture dictionary: "+qualified;return false;}
    if(image.rgba.empty()) {error="Native texture dictionary/raster is not loaded: "+qualified;return false;}
    error.clear();return true;
}
