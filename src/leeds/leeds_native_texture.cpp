#include "src/leeds/leeds_native_texture.h"
#include "src/leeds/leeds_ps2_texture_layout.h"
#include "src/leeds/leeds_psp_texture.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <cctype>

namespace {
uint32_t readWord(const std::vector<uint8_t>& data, size_t offset) {
    if (offset > data.size() || data.size() - offset < 4) return 0;
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
        (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

struct RasterLayout {
    uint32_t pointer = 0;
    uint32_t pixelBytes = 0;
    uint32_t palette=0;
    unsigned bpp = 0;
    int width = 0;
    int height = 0;
    bool swizzled = false;
    unsigned levels = 1;
    uint8_t swizzleMask = 0;
    size_t decodedBytes = 0;
    StorylandPspTextureLayout psp;
};

RasterLayout inspectRaster(const std::vector<uint8_t>& data,uint32_t node,bool vcs) {
    // RslTexture's dictionary link is at +8. Its raster pointer is at +0.
    RasterLayout layout;
    if(node<8) return layout;
    const uint32_t raster=readWord(data,size_t(node)-8);
    if (raster && uint64_t(raster) + 16u <= data.size()) {
        const unsigned bpp = data[size_t(raster) + 12u];
        const unsigned levels = data[size_t(raster) + 13u] & 63u;
        const unsigned flags = unsigned(data[size_t(raster) + 14u]) |
            (unsigned(data[size_t(raster) + 15u]) << 8u);
        const unsigned pitch = unsigned(data[size_t(raster) + 8u]) |
            (unsigned(data[size_t(raster) + 9u]) << 8u);
        const uint32_t pointer = readWord(data, size_t(raster) + 4u);
        if (((bpp == 4u && flags == 0x4500u) || (bpp == 8u && flags == 0x2500u)) &&
            pointer && !(pointer & 15u) &&
            storylandResolvePspTextureLayout(pointer, data[size_t(raster) + 10u],
                data[size_t(raster) + 11u], bpp, levels, pitch, pitch != 0u,
                data.size(), layout.psp)) {
            layout.pointer = pointer;
            layout.pixelBytes = uint32_t(layout.psp.pixelBytes);
            layout.palette = uint32_t(layout.psp.paletteOffset);
            layout.bpp = bpp;
            layout.width = layout.psp.width;
            layout.height = layout.psp.height;
            layout.swizzled = layout.psp.swizzled;
            return layout;
        }
    }
    const size_t offset=vcs?8:0;
    if(!raster||uint64_t(raster)+offset+8>data.size()) return layout;
    layout.pointer=readWord(data,size_t(raster)+offset);
    const uint32_t flags=readWord(data,size_t(raster)+offset+4);
    const unsigned logWidth=flags&63,logHeight=(flags>>6)&63;
    layout.bpp=(flags>>12)&63;
    layout.levels = std::max(1u, (flags >> 20u) & 15u);
    layout.swizzleMask = uint8_t(flags >> 24u);
    if (logWidth > 10 || logHeight > 10 || logWidth < 2 || logHeight < 2 ||
        (layout.bpp != 4 && layout.bpp != 8 && layout.bpp != 16 && layout.bpp != 32) ||
        layout.levels > 11) return {};
    layout.width = 1 << logWidth;
    layout.height = 1 << logHeight;
    layout.pixelBytes = uint32_t(size_t(layout.width) * size_t(layout.height) * layout.bpp / 8u);
    layout.swizzled = (layout.swizzleMask & 1u) != 0;
    if (!layout.pointer) return {};
    uint64_t palette = layout.pointer;
    unsigned width = layout.width, height = layout.height;
    for (unsigned level = 0; level < layout.levels; ++level) {
        const uint64_t pixels = uint64_t(width) * height;
        palette += (pixels * layout.bpp + 7u) / 8u;
        layout.decodedBytes += size_t(pixels) * 5u;
        width = std::max(1u, width / 2u);
        height = std::max(1u, height / 2u);
    }
    const unsigned paletteBytes = layout.bpp == 4 ? 64u : layout.bpp == 8 ? 1024u : 0u;
    if (palette + paletteBytes > data.size()) return {};
    layout.palette = uint32_t(palette);

    return layout;
}

RgbaImage decodeRaster(const std::vector<uint8_t>& data, const RasterLayout& layout) {
    if (layout.psp.width) return storylandDecodePspTexture(data, layout.psp);
    RgbaImage image;
    image.width = layout.width;
    image.height = layout.height;
    image.mipLevels.reserve(layout.levels - 1u);
    size_t offset = layout.pointer;
    for (unsigned level = 0; level < layout.levels; ++level) {
        RgbaMipLevel mip;
        mip.width = std::max(1, layout.width >> level);
        mip.height = std::max(1, layout.height >> level);
        const size_t pixels = size_t(mip.width) * size_t(mip.height);
        const size_t storedBytes = (pixels * layout.bpp + 7u) / 8u;
        const bool swizzled = (unsigned(layout.swizzleMask) & (1u << level)) != 0;
        mip.rgba.resize(pixels * 4u);
        mip.ps2Alpha.resize(pixels);
        if (layout.bpp == 32) {
            const size_t pitch = size_t(mip.width) * 4u;
            for (int y = 0; y < mip.height; ++y) {
                for (size_t x = 0; x < pitch; ++x) {
                    const size_t source = swizzled
                        ? size_t(y / 8) * ((pitch + 15u) / 16u) * 128u +
                            (x / 16u) * 128u + size_t(y % 8) * 16u + x % 16u
                        : size_t(y) * pitch + x;
                    if (source >= storedBytes) return {};
                    const uint8_t value = data[offset + source];
                    const size_t destination = size_t(y) * pitch + x;
                    if (x % 4u == 3u) {
                        mip.ps2Alpha[destination / 4u] = value;
                        mip.rgba[destination] = uint8_t(std::min(255u, unsigned(value) * 255u / 128u));
                    } else mip.rgba[destination] = value;
                }
            }
        } else if (layout.bpp == 16) {
            mip.ps2FiveBitColor = true;
            for (size_t pixel = 0; pixel < pixels; ++pixel) {
                const uint16_t value = uint16_t(data[offset + pixel * 2u]) |
                    uint16_t(data[offset + pixel * 2u + 1u]) << 8u;
                mip.rgba[pixel * 4u] = uint8_t((value & 31u) * 255u / 31u);
                mip.rgba[pixel * 4u + 1u] = uint8_t(((value >> 5u) & 31u) * 255u / 31u);
                mip.rgba[pixel * 4u + 2u] = uint8_t(((value >> 10u) & 31u) * 255u / 31u);
                mip.rgba[pixel * 4u + 3u] = value & 0x8000u ? 255u : 0u;
                mip.ps2Alpha[pixel] = value & 0x8000u ? 128u : 0u;
            }
        } else {
            std::vector<uint8_t> indices(pixels);
            for (size_t pixel = 0; pixel < pixels; ++pixel)
                indices[pixel] = layout.bpp == 8 ? data[offset + pixel] :
                    uint8_t(pixel & 1u ? data[offset + pixel / 2u] >> 4u :
                        data[offset + pixel / 2u] & 15u);
            if (swizzled) {
                auto linear = storylandUnswizzlePs2TextureIndices(indices, mip.width, mip.height);
                if (!linear.empty()) indices = std::move(linear);
            }
            for (size_t pixel = 0; pixel < pixels; ++pixel) {
                uint32_t color = indices[pixel];
                if (layout.bpp == 8)
                    color = (color & 0xE7u) | ((color & 0x08u) << 1u) | ((color & 0x10u) >> 1u);
                const size_t entry = size_t(layout.palette) + color * 4u;
                for (size_t channel = 0; channel < 3; ++channel)
                    mip.rgba[pixel * 4u + channel] = data[entry + channel];
                mip.ps2Alpha[pixel] = data[entry + 3u];
                mip.rgba[pixel * 4u + 3u] = uint8_t(std::min(255u, unsigned(mip.ps2Alpha[pixel]) * 2u));
            }
        }
        if (level == 0) {
            image.rgba = std::move(mip.rgba);
            image.ps2Alpha = std::move(mip.ps2Alpha);
            image.ps2FiveBitColor = mip.ps2FiveBitColor;
        } else image.mipLevels.push_back(std::move(mip));
        offset += storedBytes;
    }
    return image;
}
}

std::vector<StorylandNativeTexture> storylandDecodeNativeTextureDictionary(
    const std::vector<uint8_t>& unpackedDtz, uint32_t dictionaryOffset,
    bool viceCityStories,const std::set<std::string>* wantedNames,size_t maximumDecodedBytes) {
    std::vector<StorylandNativeTexture> decoded;
    size_t decodedBytes=0;
    if (!dictionaryOffset || size_t(dictionaryOffset) + 12 > unpackedDtz.size()) return decoded;
    const uint32_t sentinel = dictionaryOffset + 8u;
    uint32_t node = readWord(unpackedDtz, sentinel);
    std::set<uint32_t> visited;
    while (node && node != sentinel && visited.insert(node).second && visited.size() <= 512) {
        if (size_t(node) + 0x54 > unpackedDtz.size()) break;
        std::string name;
        for (size_t index = 0; index < 32 && unpackedDtz[size_t(node) + 8 + index]; ++index) {
            const auto character = unpackedDtz[size_t(node) + 8 + index];
            if (character < 32 || character > 126) { name.clear(); break; }
            name.push_back(char(character));
        }
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char ch){return char(std::tolower(ch));});
        if (!name.empty()&&(!wantedNames||wantedNames->count(name))) {
            const RasterLayout raster = inspectRaster(unpackedDtz, node, viceCityStories);
            const size_t bytes = raster.psp.width ? storylandPspTextureDecodedBytes(raster.psp) :
                raster.decodedBytes;
            if(raster.width&&decoded.size()<256&&bytes<=maximumDecodedBytes-decodedBytes) {
                auto image=decodeRaster(unpackedDtz,raster);
                const size_t imageBytes=rgbaImageDecodedBytes(image);
                if(!image.rgba.empty()&&imageBytes<=maximumDecodedBytes-decodedBytes) {
                    decodedBytes+=imageBytes;
                    decoded.push_back({std::move(name),std::move(image)});
                }
            }
        }
        node = readWord(unpackedDtz, node);
    }
    return decoded;
}

bool storylandFindNativeTexture(const std::vector<uint8_t>& bytes,bool vcs,const std::string& name,
    RgbaImage& image,size_t maximumDecodedBytes) {
    image={};
    std::string wanted=name;
    std::transform(wanted.begin(),wanted.end(),wanted.begin(),[](unsigned char ch){return char(std::tolower(ch));});
    const std::set<std::string> filter{wanted};
    const uint32_t table=readWord(bytes,0x60);
    if(!table||uint64_t(table)+12>bytes.size())return false;
    const uint32_t rows=readWord(bytes,table),count=readWord(bytes,size_t(table)+8);
    if(!rows||!count||count>4096||uint64_t(rows)+uint64_t(count)*28>bytes.size())return false;
    for(uint32_t index=0;index<count;++index) {
        const uint32_t dictionary=readWord(bytes,size_t(rows)+size_t(index)*28);
        if(!dictionary)continue;
        for(auto& texture:storylandDecodeNativeTextureDictionary(bytes,dictionary,vcs,&filter,maximumDecodedBytes))
            if(texture.name==wanted&&!texture.image.rgba.empty()) {image=std::move(texture.image);return true;}
    }
    return false;
}

std::vector<StorylandNativeTexture> storylandFindNativeTextures(const std::vector<uint8_t>& bytes,
    bool vcs, const std::set<std::string>& names, size_t maximumDecodedBytes) {
    std::vector<StorylandNativeTexture> result;
    std::set<std::string> remaining = names;
    const uint32_t table = readWord(bytes,0x60);
    if (!table || uint64_t(table)+12>bytes.size()) return result;
    const uint32_t rows=readWord(bytes,table), count=readWord(bytes,size_t(table)+8);
    if (!rows || !count || count>4096 || uint64_t(rows)+uint64_t(count)*28>bytes.size()) return result;
    size_t decodedBytes=0;
    for (uint32_t index=0; index<count && !remaining.empty() && decodedBytes<maximumDecodedBytes; ++index) {
        const uint32_t dictionary=readWord(bytes,size_t(rows)+size_t(index)*28);
        if (!dictionary) continue;
        for (auto& texture : storylandDecodeNativeTextureDictionary(bytes,dictionary,vcs,&remaining,maximumDecodedBytes-decodedBytes)) {
            if (texture.image.rgba.empty() || !remaining.erase(texture.name)) continue;
            decodedBytes+=rgbaImageDecodedBytes(texture.image);
            result.push_back(std::move(texture));
        }
    }
    return result;
}

std::vector<StorylandNativeTexture> storylandFindNativeDictionaryTextures(
    const std::vector<uint8_t>& bytes,bool vcs,const std::string& dictionaryName,
    const std::set<std::string>& names,size_t maximumDecodedBytes) {
    std::string wanted=dictionaryName;
    std::transform(wanted.begin(),wanted.end(),wanted.begin(),[](unsigned char ch){return char(std::tolower(ch));});
    if(wanted.empty()||wanted.size()>20||names.empty())return {};
    const uint32_t table=readWord(bytes,0x60);
    if(!table||uint64_t(table)+12>bytes.size())return {};
    const uint32_t rows=readWord(bytes,table),count=readWord(bytes,size_t(table)+8);
    if(!rows||!count||count>4096||uint64_t(rows)+uint64_t(count)*28>bytes.size())return {};
    for(uint32_t index=0;index<count;++index) {
        const size_t row=size_t(rows)+size_t(index)*28;
        std::string name;
        for(size_t character=0;character<20&&bytes[row+8+character];++character)
            name.push_back(char(std::tolower(bytes[row+8+character])));
        if(name!=wanted)continue;
        return storylandDecodeNativeTextureDictionary(bytes,readWord(bytes,row),vcs,
            &names,maximumDecodedBytes);
    }
    return {};
}
