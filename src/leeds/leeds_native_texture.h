#pragma once

#include "src/leeds/leeds_texture.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <set>

struct StorylandNativeTexture {
    std::string name;
    RgbaImage image;
};

std::vector<StorylandNativeTexture> storylandDecodeNativeTextureDictionary(
    const std::vector<uint8_t>& unpackedDtz, uint32_t dictionaryOffset,
    bool viceCityStories,const std::set<std::string>* wantedNames=nullptr,
    size_t maximumDecodedBytes=64u*1024u*1024u);

bool storylandFindNativeTexture(const std::vector<uint8_t>& unpackedDtz,bool viceCityStories,
    const std::string& name,RgbaImage& image,size_t maximumDecodedBytes=4u*1024u*1024u);

std::vector<StorylandNativeTexture> storylandFindNativeTextures(const std::vector<uint8_t>& unpackedDtz,
    bool viceCityStories, const std::set<std::string>& names, size_t maximumDecodedBytes=32u*1024u*1024u);

std::vector<StorylandNativeTexture> storylandFindNativeDictionaryTextures(
    const std::vector<uint8_t>& unpackedDtz,bool viceCityStories,const std::string& dictionaryName,
    const std::set<std::string>& names,size_t maximumDecodedBytes=32u*1024u*1024u);
