#include "src/storyland/storyland_ui_glow.h"
#include "src/storyland/storyland_mission_radar.h"
#include "src/storyland/storyland_player_restart.h"
#include "src/storyland/storyland_play_test_hud.h"
#include "src/storyland/storyland_loading_screens.h"
#include "src/storyland/storyland_inverted_map.h"
#include "src/storyland/storyland_play_test_camera.h"
#include "src/re/reLCS2/runtime/storyland_lcs_pickup_presentation.h"
#include "src/re/reLCS2/runtime/storyland_lcs_host_queries.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/leeds/leeds_native_texture.h"
#include "src/storyland/storyland_hud_geometry.h"
#include "src/storyland/storyland_hud_cluster_layout.h"
#include "src/re/reLCS2/hud/Layout.h"
#include "src/re/reVCS/hud/Hud.h"
#include "src/storyland/storyland_hud_names.h"
#include "src/storyland/storyland_script_corona.h"
#include "src/storyland/storyland_gxt.h"
#include "src/re/reVCS/runtime/storyland_vcs_clock_event.h"
#include "src/re/reVCS/hud/storyland_vcs_hud_components.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <set>
#include <zlib.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include "src/graphics/storyland_directx.h"
#elif defined(__ANDROID__)
#include <SDL.h>
#include <GLES3/gl3.h>
#elif defined(__linux__)
#include <SDL.h>
#include <GL/gl.h>
#endif

#if defined(_WIN32) || defined(__linux__) || defined(__ANDROID__)
#define STORYLAND_NATIVE_HUD_OPENGL 1
#include "src/graphics/storyland_texture_upload.h"
#endif

namespace {
#ifdef _WIN32
using StorylandHudContext=HGLRC;
StorylandHudContext storylandCurrentHudContext() {return wglGetCurrentContext();}
#elif defined(STORYLAND_NATIVE_HUD_OPENGL)
using StorylandHudContext=SDL_GLContext;
StorylandHudContext storylandCurrentHudContext() {return SDL_GL_GetCurrentContext();}
#endif
}

namespace {
std::string lowerHudName(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}
std::filesystem::path findHudDirectoryEntry(const std::filesystem::path& folder,
    const std::string& name,bool directory) {
    std::error_code error;
    std::filesystem::directory_iterator cursor(folder,
        std::filesystem::directory_options::skip_permission_denied,error),end;
    for(;!error&&cursor!=end;cursor.increment(error)) {
        if(lowerHudName(cursor->path().filename().string())!=name)continue;
        std::error_code entryError;
        const bool matches=directory?cursor->is_directory(entryError):cursor->is_regular_file(entryError);
        if(matches&&!entryError)return cursor->path();
    }
    return {};
}
std::string hudSpriteRole(const std::string& name) {
    const std::string lower = lowerHudName(name);
    for(int type=0;type<9;++type)if(lower==storylandCoronaTextureName(type))return {};
    if (lower.find("armour") != std::string::npos || lower.find("armor") != std::string::npos || lower.find("shield") != std::string::npos) return "armour";
    if (lower.find("heart") != std::string::npos || lower.find("health") != std::string::npos) return "health";
    if (lower.find("star") != std::string::npos || lower.find("wanted") != std::string::npos) return "star";
    if (lower.find("clock") != std::string::npos || lower.find("watch") != std::string::npos) return "clock";
    if (lower.find("fist") != std::string::npos || lower.find("unarmed") != std::string::npos) return "fist";
    if (lower == "font1" || lower == "font2" || lower == "font3") return lower;
    if (lower == "gametext" || lower == "fancytext" || lower == "bigtext" || lower == "pagertext" || lower == "hudnumbers") return lower;
    if (lower == "radardisc" || lower == "radar_disc") return "radardisc";
    if (lower == "radardiscback") return "radardiscback";
    if (lower == "radar_balloon") return "radar_balloon";
    if (lower == "radar_centre" || lower == "radar_north" || lower == "mp_player") return lower;
    if (lower == "mp_car") return "mp_car";
    if(lower=="bar_inside1")return "bar_inside1dark";
    if(lower=="bar_inside2")return "bar_inside2dark";
    if (lower == "bar_inside1dark" || lower == "bar_outline" || lower == "bar_inside2dark") return lower;
    if (lower == "gun") return "gun";
    const auto key=storylandHudKey(name);
    const auto station=storylandRadioLogoKey(name);
    if(!station.empty())return "radio:"+station;
    if(key.size()>4&&key.compare(key.size()-4,4,"icon")==0)
        return "weapon:"+storylandWeaponHudKey(key.substr(0,key.size()-4));
    if(key.compare(0,6,"weapon")==0&&key.size()>6)return "weapon:"+storylandWeaponHudKey(key.substr(6));
    return {};
}
std::string nativeHudSpriteRole(const std::string& name,const char* dictionary) {
    if(!dictionary||(std::strcmp(dictionary,"hud")!=0&&
        std::strcmp(dictionary,"fsfonts")!=0&&std::strcmp(dictionary,"fonts")!=0))return {};
    return std::string(dictionary)+":"+name;
}
uint32_t hudU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) return 0;
    return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8) |
        (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
}
uint32_t hudNativeTextureHash(const std::string& name) {
    std::string upper=name;
    for(char& character:upper)
        character=char(std::toupper(static_cast<unsigned char>(character)));
    return uint32_t(crc32(0,reinterpret_cast<const Bytef*>(upper.data()),
        uInt(upper.size())))^0xFFFFFFFFu;
}
bool inflateHudResource(const std::vector<uint8_t>& data, size_t offset,
    std::vector<uint8_t>& output) {
    output.clear();
    if (offset >= data.size() || data.size() - offset < 8 ||
        data[offset] != 0x78u) return false;
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK) return false;
    std::unique_ptr<z_stream,decltype(&inflateEnd)> cleanup(&stream,inflateEnd);
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data() + offset));
    stream.avail_in = uInt(std::min(size_t(16u * 1024u * 1024u), data.size() - offset));
    std::vector<uint8_t> chunk(65536);
    bool valid = false;
    while (output.size() < 16u * 1024u * 1024u) {
        stream.next_out = chunk.data();
        stream.avail_out = uInt(chunk.size());
        const int result = inflate(&stream, Z_NO_FLUSH);
        const size_t produced = chunk.size() - stream.avail_out;
        if (produced > 16u * 1024u * 1024u - output.size()) break;
        output.insert(output.end(), chunk.begin(), chunk.begin() + produced);
        if (result == Z_STREAM_END) { valid = true; break; }
        if (result != Z_OK || produced == 0) break;
    }
    if (!valid || output.size() < 0x40u) output.clear();
    return valid && !output.empty();
}
}

bool StorylandRetailHudAssets::loadUserTracksIcon(const std::wstring& path, std::string& error) {
    LeedsTextureArchive archive;
    if (!archive.loadFromFile(path, LeedsPlatform::Auto, error)) return false;
    return loadUserTracksIcon(archive, error);
}

bool StorylandRetailHudAssets::loadUserTracksIcon(const std::vector<uint8_t>& bytes, std::string& error) {
    LeedsTextureArchive archive;
    if (!archive.loadFromMemory(bytes, LeedsPlatform::Auto, error, L"usertracks.xtx")) return false;
    return loadUserTracksIcon(archive, error);
}

bool StorylandRetailHudAssets::loadUserTracksIcon(const LeedsTextureArchive& archive, std::string& error) {
    const auto& textures = archive.textures();
    for (size_t index = 0; index < textures.size(); ++index) {
        if (storylandRadioLogoKey(textures[index].name) != "usertracks" && textures.size() != 1) continue;
        if (textures[index].width < 4 || textures[index].height < 4 ||
            textures[index].width > 1024 || textures[index].height > 1024) continue;
        RgbaImage icon;
        if (!archive.decodeTexture(index, icon, error)) return false;
        if (icon.rgba.size() != size_t(icon.width) * size_t(icon.height) * 4) {
            error = "User Tracks XTX has an incomplete raster.";
            return false;
        }
        sprites["radio:usertracks"] = std::make_shared<const RgbaImage>(std::move(icon));
        ++generation;
        error.clear();
        return true;
    }
    error = "User Tracks XTX has no usable usertracks texture.";
    return false;
}

bool StorylandRetailHudAssets::loadUserTracksIconNear(const std::wstring& directory, std::string& error) {
    auto folder = std::filesystem::path(directory);
    for (unsigned level = 0; level < 5 && !folder.empty(); ++level) {
        const auto candidate = folder / "assets" / "radio" / "usertracks.xtx";
        std::error_code status;
        if (std::filesystem::is_regular_file(candidate, status))
            return loadUserTracksIcon(candidate.wstring(), error);
        const auto parent = folder.parent_path();
        if (parent == folder) break;
        folder = parent;
    }
    error = "assets/radio/usertracks.xtx was not found beside the application.";
    return false;
}

void StorylandRetailHudAssets::load(const StorylandDtzArchive& archive,
    const std::vector<std::wstring>& looseCandidates) {
    const bool vcs=archive.gameVariant()==StorylandDtzGameVariant::Vcs;
    auto hudCandidates=looseCandidates;
    if(vcs&&!archive.sourcePath().empty()) {
        const auto folder=std::filesystem::path(archive.sourcePath()).parent_path();
        std::vector<std::filesystem::path> roots{folder};
        const auto name=lowerHudName(folder.filename().string());
        if(name=="ps2"||name=="psp")roots.push_back(folder.parent_path());
        for(const auto& root:roots) {
            // SLUS_215.90 1067F4..10680C loads cdrom0:/RUNDATA/EMPHUD.XTX.
            const auto runtime=findHudDirectoryEntry(root,"rundata",true);
            if(runtime.empty())continue;
            const auto file=findHudDirectoryEntry(runtime,"emphud.xtx",false);
            if(!file.empty()&&std::find(hudCandidates.begin(),hudCandidates.end(),file.wstring())==hudCandidates.end())
                hudCandidates.push_back(file.wstring());
        }
    }
    // A new run can reuse the decoded HUD when every contributing source matches.
    uint64_t fingerprint=1469598103934665603ull;
    const auto hashByte=[&](uint8_t value){fingerprint=(fingerprint^value)*1099511628211ull;};
    const auto revision=archive.contentRevision();
    for(unsigned byte=0;byte<8;++byte)hashByte(uint8_t(revision>>(8*byte)));
    const auto hashFile=[&](const std::filesystem::path& file) {
        for(auto value:file.native()) {const auto bits=uint32_t(value);for(unsigned byte=0;byte<4;++byte)hashByte(uint8_t(bits>>(8*byte)));}
        std::error_code error;
        const auto time=std::filesystem::last_write_time(file,error);
        const uint64_t stamp=error?0:uint64_t(time.time_since_epoch().count());
        const auto size=std::filesystem::file_size(file,error);
        const uint64_t length=error?0:uint64_t(size);
        for(unsigned byte=0;byte<8;++byte) {hashByte(uint8_t(stamp>>(8*byte)));hashByte(uint8_t(length>>(8*byte)));}
    };
    hashFile(archive.sourcePath());
    hashFile(archive.companionImgPath());
    for(const auto& file:hudCandidates)hashFile(file);
    fingerprint|=1;
    if(loadedFingerprint==fingerprint&&!sprites.empty())return;
    const bool requestedPcMode=pcMode;
    pcMode=false;
    pcSpriteBindings.clear();
    pcFontBindings.clear();
    loadedFingerprint=0;
    sprites.clear();radarTiles.clear();radarAtlas=false;radarGrid=8;radarTileSize=500;radarWorldMinimum=-2000;source.clear();++generation;
    fontSprites=menuSprites=splashSprites=0;
    const auto& data=archive.unpackedBytes();
    std::set<std::string> weaponDictionaries;
    std::map<uint32_t,std::vector<int32_t>> weaponHashes;
    std::map<int32_t,std::string> weaponOwners;
    {
        const uint32_t modelCount=hudU32(data,0x38),models=hudU32(data,0x3C);
        const uint32_t textureOwner=hudU32(data,0x60);
        const uint32_t textureRows=hudU32(data,textureOwner);
        const uint32_t textureCount=hudU32(data,size_t(textureOwner)+8);
        uint32_t weaponTable=0,weaponCount=0;
        if(vcs) {
            for(const auto& block:archive.dataBlocks()) {
                if(block.parser!="vcs-weapon-0x70-records"||block.rowSize!=0x70||block.rowCount>64)continue;
                weaponTable=block.offset;weaponCount=block.rowCount;break;
            }
        } else {
            const uint32_t owner=hudU32(data,0xBC);
            const uint32_t table=owner>=0x20?hudU32(data,owner):0;
            if(table>=0x20&&uint64_t(table)+37u*0x70u<=data.size()&&
                int32_t(hudU32(data,size_t(table)+0x60))==-1) {
                weaponTable=table;weaponCount=37;
                for(uint32_t weapon=0;weapon<weaponCount;++weapon) {
                    const size_t row=size_t(table)+weapon*0x70u;
                    if(hudU32(data,row)>4||hudU32(data,row+0x68)>=10) {
                        weaponCount=0;break;
                    }
                }
            }
        }
        if(weaponTable>=0x20&&uint64_t(weaponTable)+uint64_t(weaponCount)*0x70u<=data.size()) {
            for(uint32_t weapon=1;weapon<weaponCount;++weapon) {
                const int32_t model=int32_t(hudU32(data,size_t(weaponTable)+weapon*0x70u+0x60u));
                if(model<0||uint32_t(model)>=modelCount||modelCount>200000||models<0x20||
                    uint64_t(models)+uint64_t(modelCount)*4>data.size())continue;
                const uint32_t info=hudU32(data,size_t(models)+uint32_t(model)*4u);
                if(info<0x20||uint64_t(info)+0x20>data.size())continue;
                weaponHashes[hudU32(data,size_t(info)+8)].push_back(int32_t(weapon));
                const int16_t dictionary=int16_t(uint16_t(data[size_t(info)+0x1E])|
                    uint16_t(data[size_t(info)+0x1F])<<8);
                if(dictionary<0||uint32_t(dictionary)>=textureCount||textureCount>4096||
                    textureRows<0x20||uint64_t(textureRows)+uint64_t(textureCount)*28>data.size())continue;
                std::string name;
                const size_t first=size_t(textureRows)+size_t(dictionary)*28+8;
                for(size_t index=0;index<20&&data[first+index];++index)
                    name.push_back(char(data[first+index]));
                if(!name.empty()) {
                    const auto dictionaryName=lowerHudName(name);
                    weaponDictionaries.insert(dictionaryName);
                    weaponOwners[int32_t(weapon)]=dictionaryName;
                }
            }
        }
    }
    auto store=[&](const std::string& textureName,RgbaImage image,const std::string& origin,
        const char* nativeDictionary=nullptr) {
        if(image.width<4||image.height<4||image.width>2048||image.height>2048||
            image.rgba.size()!=size_t(image.width)*size_t(image.height)*4)return;
        const auto role=hudSpriteRole(textureName);
        const auto name=lowerHudName(textureName);
        const auto stem=lowerHudName(std::filesystem::path(origin).stem().string());
        const auto textureKey=storylandHudKey(textureName);
        auto pixels=std::make_shared<const RgbaImage>(std::move(image));
        const auto nativeRole=nativeHudSpriteRole(name,nativeDictionary);
        if(!nativeRole.empty())sprites.emplace(nativeRole,pixels);
        if(stem.find("font")!=std::string::npos||role=="font1"||role=="font2"||role=="font3"||
            role=="fancytext"||role=="gametext"||name=="bigtext"||name=="pagertext")++fontSprites;
        if(stem.find("menu")!=std::string::npos||stem=="emphud")++menuSprites;
        if(stem.find("loadsc")==0||stem.find("splash")==0) {sprites.emplace("splash:"+stem,pixels);++splashSprites;}
        if(origin=="GAME.DTZ:particle")sprites.emplace("particle:"+name,pixels);
        const bool existingRole=sprites.count(role)!=0;
        sprites.emplace(name,pixels);
        if(!weaponHashes.empty()) {
            const auto weapons=weaponHashes.find(hudNativeTextureHash(textureName));
            if(weapons!=weaponHashes.end())for(int32_t weapon:weapons->second) {
                const auto owner=weaponOwners.find(weapon);
                if(owner!=weaponOwners.end()&&owner->second==stem)
                    sprites.emplace("weapon:native:"+std::to_string(weapon),pixels);
            }
        }
        if(name=="bar_inside1"||name=="bar_inside2"||
            name=="bar_inside1dark"||name=="bar_inside2dark") {
            sprites[name]=pixels;
        } else if(!role.empty()&&(!existingRole||role=="bar_inside1dark"||role=="bar_inside2dark")) {
            if(role=="bar_inside1dark"||role=="bar_inside2dark") {
                auto mask=std::make_shared<RgbaImage>(*pixels);
                for(size_t pixel=0;pixel<mask->rgba.size();pixel+=4) {
                    const int intensity=std::max({int(mask->rgba[pixel]),int(mask->rgba[pixel+1]),int(mask->rgba[pixel+2])});
                    mask->rgba[pixel+3]=uint8_t(int(mask->rgba[pixel+3])*intensity/255);
                    mask->rgba[pixel]=mask->rgba[pixel+1]=mask->rgba[pixel+2]=255;
                }
                sprites[role]=std::move(mask);
            } else sprites.emplace(role,pixels);
        }
        int tile=storylandRadarTileIndex(textureName);
        if(tile<0)tile=storylandRadarTileIndex(stem);
        if(tile<0&&(textureKey=="radar"||textureKey=="radarmap")&&pixels->width>=512&&pixels->height>=512&&radarTiles.empty()) {
            sprites.emplace("radar:full",pixels);radarTiles[0]="radar:full";
            radarAtlas=true;radarGrid=1;radarTileSize=4000;radarWorldMinimum=-2000;
        }
        if(tile>=0) {
            if(radarAtlas) {radarTiles.clear();radarAtlas=false;radarGrid=8;radarTileSize=500;}
            const std::string key="radar:"+std::to_string(tile);
            sprites.emplace(key,pixels);radarTiles[tile]=key;
            radarGrid=std::max(8,int(std::ceil(std::sqrt(float(tile+1)))));
            radarWorldMinimum=-float(radarGrid)*radarTileSize*0.5f;
        }
        source=origin;
    };
    auto absorb = [&](LeedsTextureArchive& textures,const std::string& origin,bool fallback=false,
        const char* nativeDictionary=nullptr) {
        const auto& entries=textures.textures();
        const auto dictionary=lowerHudName(std::filesystem::path(origin).stem().string());
        for(size_t index=0;index<entries.size()&&index<512;++index) {
            const auto name=lowerHudName(entries[index].name);
            const auto role=hudSpriteRole(name);
            const auto nativeRole=nativeHudSpriteRole(name,nativeDictionary);
            const bool nativeRoleMissing=!nativeRole.empty()&&!sprites.count(nativeRole);
            const int tile=storylandRadarTileIndex(name);
            if(fallback&&role.empty()&&tile<0&&name!="radar"&&name!="radarmap"&&
                name!="bigtext"&&name!="pagertext"&&name!="frontend_background")continue;
            bool nativeWeaponMissing=false;
            const auto weapons=weaponHashes.find(hudNativeTextureHash(entries[index].name));
            if(weapons!=weaponHashes.end())for(int32_t weapon:weapons->second) {
                const auto owner=weaponOwners.find(weapon);
                if(owner!=weaponOwners.end()&&owner->second==dictionary&&
                    !sprites.count("weapon:native:"+std::to_string(weapon))) {
                    nativeWeaponMissing=true;
                    break;
                }
            }
            if(role!="bar_inside1dark"&&role!="bar_inside2dark"&&sprites.count(name)&&(role.empty()||sprites.count(role))&&
                (tile<0||radarTiles.count(tile))&&!nativeWeaponMissing&&!nativeRoleMissing)continue;
            // Check before decode allocates its RGBA output.
            if(entries[index].width<4||entries[index].height<4||entries[index].width>2048||entries[index].height>2048)continue;
            RgbaImage image;std::string error;
            if(textures.decodeTexture(index,image,error))
                store(entries[index].name,std::move(image),origin,nativeDictionary);
        }
    };
    for (const auto& candidate : hudCandidates) {
        const auto path = std::filesystem::path(candidate);
        const auto name = lowerHudName(path.stem().string());
        if(!storylandHudDictionaryWanted(name)&&!weaponDictionaries.count(name)&&name.find("loadsc")!=0&&
            name.find("splash")!=0&&name!="emphud")continue;
        LeedsTextureArchive textures; std::string error;
        if (textures.loadFromFile(candidate, LeedsPlatform::Auto, error))
            absorb(textures, path.filename().string());
    }
    const auto& directory=archive.dirEntries();
    for(size_t index=0;index<directory.size();++index) {
        const auto& entry=directory[index];
        const auto stem=lowerHudName(std::filesystem::path(entry.name).stem().string());
        if(!storylandHudDictionaryWanted(stem)&&!weaponDictionaries.count(stem))continue;
        if(entry.detectedExtension!=".chk"&&entry.detectedExtension!=".xtx"&&entry.detectedExtension!=".txd") continue;
        std::vector<uint8_t> bytes;std::string error;
        LeedsTextureArchive textures;
        if(archive.extractDirEntryBytes(index,bytes,error)&&
           textures.loadFromMemory(bytes,LeedsPlatform::Auto,error)) absorb(textures,entry.name);
    }
    // Retail VCS: +0xE0 points to the compressed menu TXD (33 textures),
    // +0xFC points to the four font atlases (fancy/game/big/pager text).
    // The previous +0xD4/+0xDC fields are null in the shipped GAME.DTZ.
    std::set<std::string> coronaNames;
    for (int type = 0; type < 9; ++type) coronaNames.insert(storylandCoronaTextureName(type));
    for (const auto& effect : archive.leeds2dfxEffects())
        if (effect.isLight && !effect.coronaTextureName.empty()) coronaNames.insert(effect.coronaTextureName);
    for (const auto& name : coronaNames) {
        RgbaImage image;
        if (storylandFindNativeTexture(data, vcs, name, image))
            store(name, std::move(image), "GAME.DTZ:corona");
    }
    fonts=storylandDecodeRetailFonts(data,vcs);
    {
        // Both GTA Stories games ship their gun flashes in GAME.DTZ's native
        // particle dictionary; they are not just VCS HUD sprites.
        const std::set<std::string> particleNames{"triangle16","muzzleflash","muzzleflashback",
            "gunflash1","gunflash2","gunflash3","gunflash4","flame1","flame2","flame3","flame4","flame5",
            "coronastar","corona","coronaring","shad_exp","shad_car","shad_bike","shad_ped","headlight_single",
            "explo01","explo02","explo03","explo04","explo05","explo06",
            "smokeii_3","smoke1","smoke2","smoke3","smoke4","smoke5",
            "cloudmasked","collisionsmoke",
            "rubber1","rubber2","rubber3","rubber4","rubber5",
            "blood","bloodpool_64","bloodsplat2","skid","gameleaf01_64","gameleaf02_64",
            "newspaper01_64","newspaper02_64",
            "boatwake2","raindrop4","rainsmall",
            "splash1","splash2","splash3","splash4","splash5","splash_up1","splash_up2",
            "waterspray1","waterspray2","waterspray3","swimwake","swimwake2","firehose","carsplash_01","carsplash_02","carsplash_03","carsplash_04",
            "dollar","zonewhite64","xv_white_sl","ringborderblur"};
        for(auto& texture:storylandFindNativeDictionaryTextures(data,vcs,"particle",particleNames))
            store(texture.name,std::move(texture.image),"GAME.DTZ:particle");
        for(const auto* name:{"zonewhite64","xv_white_sl","ringborderblur"}) {
            if(sprites.count(name))continue;
            RgbaImage image;
            if(storylandFindNativeTexture(data,vcs,name,image))
                store(name,std::move(image),"GAME.DTZ:marker");
        }
    }
    std::set<size_t> compressedOffsets;
    std::set<uint32_t> nativeDictionaries;
    for (const size_t header : (vcs?std::array<size_t,2>{0xE0,0xFC}:
        std::array<size_t,2>{0xD4,0xDC})) {
        const uint32_t pointer=hudU32(data,header);
        if(!pointer || pointer>=data.size()||!compressedOffsets.insert(pointer).second)continue;
        std::vector<uint8_t> unpacked;
        if(!inflateHudResource(data,pointer,unpacked))continue;
        LeedsTextureArchive textures;std::string error;
        if(textures.loadFromMemory(unpacked,LeedsPlatform::Ps2,error,L"GAME.DTZ:CHK")) {
            const bool fontDictionary=header==(vcs?0xFCu:0xDCu);
            absorb(textures,fontDictionary?"GAME.DTZ:fonts.chk":"GAME.DTZ:menu.chk",false,
                fontDictionary?(vcs?"fsfonts":"fonts"):nullptr);
        }
    }
    if(!vcs) {
        const char* names[]={"fancytext","gametext","bigtext"};
        const char* textures[]={"font2","font1","font3"};
        for(unsigned style=0;style<3;++style) {
            auto found=sprites.find(std::string("fonts:")+textures[style]);
            if(found==sprites.end())found=sprites.find(textures[style]);
            if(found==sprites.end()||!found->second)continue;
            const auto& image=*found->second;
            fonts[names[style]]=storylandLcsRetailFont(style,uint16_t(image.width),uint16_t(image.height));
            sprites[names[style]]=found->second;
        }
        auto wanted=sprites.find("fonts:font3");
        if(wanted==sprites.end())wanted=sprites.find("font3");
        if(wanted!=sprites.end()&&wanted->second) {
            const auto& image=*wanted->second;
            auto font=storylandLcsHudWantedFont(uint16_t(image.width),uint16_t(image.height));
            if(!font.glyphs.empty()) {
                fonts["hudwanted"]=std::move(font);
                sprites["hudwanted"]=wanted->second;
            }
        }
    }
    const uint32_t texList = hudU32(data, 0x60u);
    if (texList && size_t(texList) + 12u <= data.size()) {
        const uint32_t rows = hudU32(data, texList);
        const uint32_t count = hudU32(data, size_t(texList) + 8u);
        if (count > 0 && count < 100000 && rows &&
            uint64_t(rows) + uint64_t(count) * 28ull <= data.size()) {
            for (uint32_t index = 0; index < count; ++index) {
                const size_t row = size_t(rows) + size_t(index) * 28u;
                std::string resourceName;
                for (size_t character = 0; character < 20 && data[row + 8u + character]; ++character)
                    resourceName.push_back(char(data[row + 8u + character]));
                resourceName = lowerHudName(resourceName);
                if(!storylandHudDictionaryWanted(resourceName)&&!weaponDictionaries.count(resourceName))continue;
                const uint32_t dictionary = hudU32(data, row);
                if (dictionary&&nativeDictionaries.insert(dictionary).second) {
                    const bool vcs = archive.gameVariant()==StorylandDtzGameVariant::Vcs;
                    for (auto& native : storylandDecodeNativeTextureDictionary(data, dictionary, vcs)) {
                        store(native.name,std::move(native.image),resourceName,resourceName.c_str());
                    }
                }
                for (size_t field : {size_t(0), size_t(4)}) {
                    const uint32_t offset = hudU32(data, row + field);
                    if (!offset || offset >= data.size()||!compressedOffsets.insert(offset).second) continue;
                    std::vector<uint8_t> inflated;
                    if (!inflateHudResource(data, offset, inflated)) continue;
                    LeedsTextureArchive textures; std::string error;
                    if (textures.loadFromMemory(inflated, LeedsPlatform::Ps2, error, L"GAME.DTZ:HUD"))
                        absorb(textures,resourceName,false,resourceName.c_str());
                }
            }
        }
    }
    if(vcs)for(const auto* name:{"fancytext","gametext","bigtext","pagertext"}) {
        const auto found=sprites.find(std::string("fsfonts:")+name);
        if(found!=sprites.end()&&found->second)sprites[name]=found->second;
    }
    {
        std::set<std::string> names;
        for(int32_t sprite=1;sprite<(vcs?55:64);++sprite) {
            const std::string name=storylandRadarSpriteName(vcs,sprite);
            if(name.empty())continue;
            if(!sprites.count(name))names.insert(name);
        }
        for(const auto* name:{"fist","gren_rem","hudnumbers","bar_inside1","bar_inside2",
            "bar_inside1dark","bar_inside2dark","bar_outline"})
            if(!sprites.count(name))names.insert(name);
        for(auto& texture:storylandFindNativeTextures(data,vcs,names))
            store(texture.name,std::move(texture.image),"GAME.DTZ:native HUD");
    }
    if(vcs) {
        for(auto& screen:storylandReadVcsSplashScreens()) {
            const std::string key="splash:"+screen.name;
            const bool inserted=sprites.count(key)==0;
            sprites[key]=std::make_shared<const RgbaImage>(std::move(screen.image));
            if(inserted)++splashSprites;
        }
    }
    auto bindNumbers=[&]() {
        const auto numbers=sprites.find("hudnumbers");
        if(numbers!=sprites.end()&&numbers->second) {
            const auto& image=*numbers->second;
            fonts["hudnumbers"]=storylandVcsHudNumberFont(uint16_t(image.width),uint16_t(image.height),false);
            fonts["hudmoney"]=storylandVcsHudNumberFont(uint16_t(image.width),uint16_t(image.height),true);
            sprites["hudmoney"]=numbers->second;
        }
    };
    bindNumbers();
    if(sprites.count("fist")&&sprites.count("star")&&sprites.count("bar_outline")&&sprites.count("radardisc")) {
        loadPcAssets(vcs);
        setPcMode(requestedPcMode);
        loadedFingerprint=fingerprint;
        return;
    }
    size_t candidates = 0;
    const size_t limit = std::min(data.size(), size_t(192u * 1024u * 1024u));
    for (size_t offset = 0; offset + 0x40u < limit && candidates < 48; offset += 4) {
        if (data[offset] != 'x' || data[offset + 1] != 'e' || data[offset + 2] != 't') continue;
        const uint32_t length = hudU32(data, offset + 8);
        if (length < 0x40u || length > 16u * 1024u * 1024u || length > data.size() - offset) continue;
        ++candidates;
        std::vector<uint8_t> bytes(data.begin() + offset, data.begin() + offset + length);
        LeedsTextureArchive textures; std::string error;
        if (textures.loadFromMemory(bytes, LeedsPlatform::Ps2, error, L"GAME.DTZ:HUD"))
            absorb(textures, "GAME.DTZ",true);
    }
    bindNumbers();
    loadPcAssets(vcs);
    setPcMode(requestedPcMode);
    loadedFingerprint=fingerprint;
}

void storylandDrawLcsNavigationArrow(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& world,const StorylandNativePoint& player,float cameraYaw,ImVec2 viewport) {
    if(!draw||!world.lcsNavigationArrowVisible||viewport.x<240||viewport.y<180)return;
    StorylandNavigationDirection direction;
    if(!storylandNavigationDirection(world.lcsNavigationArrowTarget,{player.x,player.y,player.z},cameraYaw,direction))return;
    // The surviving LCS source has the target command but no cNavArrow renderer.
    // Use the same camera-relative basis as the radar for a functional HUD arrow.
    const float scale=std::clamp(viewport.y/448.0f,0.65f,3.0f);
    const ImVec2 center(viewport.x*0.5f,viewport.y*0.20f);
    const ImU32 tint=IM_COL32(124,178,225,255);
    draw->AddCircleFilled(center,28*scale,IM_COL32(0,0,0,155),32);
    if(direction.distance<=2) {
        draw->AddCircle(center,12*scale,tint,24,3*scale);
        draw->AddCircleFilled(center,3*scale,tint,12);
    } else {
        const auto point=[&](float forward,float side) {
            return ImVec2(center.x+(direction.x*forward-direction.y*side)*scale,
                center.y+(direction.y*forward+direction.x*side)*scale);
        };
        draw->AddTriangleFilled(point(21,0),point(1,-13),point(1,13),tint);
        draw->AddQuadFilled(point(2,-5),point(-18,-5),point(-18,5),point(2,5),tint);
    }
    if(std::abs(direction.height)>3) {
        const float side=center.x+38*scale;
        const float sign=direction.height>0?-1.0f:1.0f;
        draw->AddTriangleFilled(ImVec2(side,center.y+sign*7*scale),
            ImVec2(side-5*scale,center.y-sign*3*scale),ImVec2(side+5*scale,center.y-sign*3*scale),tint);
    }
    char distance[64]{};
    if(direction.distance<1000)std::snprintf(distance,sizeof(distance),"%.0f m",double(direction.distance));
    else std::snprintf(distance,sizeof(distance),"%.1f km",double(direction.distance)/1000.0);
    const ImVec2 label(center.x,center.y+31*scale);
    storylandDrawPlayTestText(draw,assets,ImVec2(label.x+scale,label.y+scale),IM_COL32_BLACK,
        distance,16*scale,0,"gametext",true);
    storylandDrawPlayTestText(draw,assets,label,tint,distance,16*scale,0,"gametext",true);
}

static void drawScriptFrameText(ImDrawList* draw, const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& mission, ImVec2 displaySize, bool late);
namespace {
std::string scriptTextUtf8(const std::u16string& value);
uint32_t readRetailUtf8(const unsigned char*& cursor);
void drawScriptTextLayout(ImDrawList* draw,const StorylandRetailHudAssets* assets,const char* fontName,
    const StorylandScriptTextLayout& layout,ImVec2 scale,ImVec2 offset);
}

static void drawLcsMissionRadarTrace(ImDrawList* draw,ImVec2 point,int16_t extent,
    StorylandRadarHeight height,const std::array<uint8_t,4>& color,ImVec2 scale,bool widescreen) {
    const uint8_t nativeHeight=height==StorylandRadarHeight::Above?0:
        height==StorylandRadarHeight::Below?1:2;
    const auto primitives=storylandLcsRadarTrace(0,0,extent,nativeHeight,color,widescreen);
    const auto flags=draw->Flags;
    draw->Flags&=~ImDrawListFlags_AntiAliasedFill;
    for(const auto& primitive:primitives) {
        std::array<ImVec2,4> vertices;
        for(size_t index=0;index<vertices.size();++index)
            vertices[index]=ImVec2(point.x+primitive.points[index][0]*scale.x,
                point.y+primitive.points[index][1]*scale.y);
        const auto& rgba=primitive.color;
        const ImU32 tint=IM_COL32(rgba[0],rgba[1],rgba[2],rgba[3]);
        if(primitive.points[0]==primitive.points[1])
            draw->AddTriangleFilled(vertices[0],vertices[2],vertices[3],tint);
        else if(primitive.points[2]==primitive.points[3])
            draw->AddTriangleFilled(vertices[0],vertices[1],vertices[2],tint);
        else draw->AddQuadFilled(vertices[0],vertices[1],vertices[2],vertices[3],tint);
    }
    draw->Flags=flags;
}

static void drawVcsMissionRadarTrace(ImDrawList* draw,ImVec2 point,uint8_t extent,
    StorylandRadarHeight height,const std::array<uint8_t,4>& color,ImVec2 scale,bool alternate) {
    const uint8_t nativeHeight=height==StorylandRadarHeight::Above?0:
        height==StorylandRadarHeight::Below?1:2;
    const auto primitives=storylandVcsRadarTrace(0,0,extent,nativeHeight,color,alternate);
    const auto flags=draw->Flags;
    draw->Flags&=~ImDrawListFlags_AntiAliasedFill;
    for(const auto& primitive:primitives) {
        std::array<ImVec2,4> vertices;
        for(size_t i=0;i<vertices.size();++i)
            vertices[i]=ImVec2(point.x+primitive.points[i][0]*scale.x,
                point.y+primitive.points[i][1]*scale.y);
        const auto& rgba=primitive.color;
        const ImU32 tint=IM_COL32(rgba[0],rgba[1],rgba[2],rgba[3]);
        if(primitive.points[0]==primitive.points[1])
            draw->AddTriangleFilled(vertices[0],vertices[2],vertices[3],tint);
        else if(primitive.points[2]==primitive.points[3])
            draw->AddTriangleFilled(vertices[0],vertices[1],vertices[2],tint);
        else draw->AddQuadFilled(vertices[0],vertices[1],vertices[2],vertices[3],tint);
    }
    draw->Flags=flags;
}

void storylandDrawPlayTestHud(const StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const std::vector<StorylandNativeTrafficActor>& cars,
    const StorylandNativePoint& player,float heading,float cameraYaw,bool vcs,bool debugMenuOpen,bool cutsceneActive,
    const StorylandRetailHudAssets* retailHud,StorylandMissionEnvironment* mission,const StorylandStoriesCamera* camera,float cinematicViewportTop,
    bool invertedMap,StorylandRadarOverlay radarOverlay,const StorylandPcHelpBindings* helpBindings) {
    const auto size=ImGui::GetIO().DisplaySize;
    if(size.x<240||size.y<180) return;
    auto* draw=ImGui::GetForegroundDrawList();
    const ImU32 accent=vcs?IM_COL32(244,112,196,255):IM_COL32(124,178,225,255);
    const float left=22;
    const bool retailCutsceneHudSuppressed = cutsceneActive ||
        (mission && (mission->widescreen || mission->cutscene.playing));
    if(!retailCutsceneHudSuppressed && runtime.playerRestart.active()) {
        const auto presentation=storylandPlayerRestartPresentation(runtime.playerRestart);
        if(presentation.showMessage) {
            const float height=std::clamp(size.y*0.075f,30.0f,64.0f);
            std::string label=mission?scriptTextUtf8(storylandMissionFormattedText(*mission,presentation.messageKey)):std::string{};
            if(label.empty()||label==presentation.messageKey)
                label=runtime.playerRestart.cause==StorylandPlayerRestartCause::Arrest?"Busted!":"Wasted!";
            const auto extent=storylandMeasurePlayTestText(retailHud,label.c_str(),height,size.x,"bigtext");
            const ImVec2 origin((size.x-extent.x)*0.5f,size.y*0.43f);
            storylandDrawPlayTestText(draw,retailHud,ImVec2(origin.x+2,origin.y+2),IM_COL32_BLACK,label.c_str(),height,size.x,"bigtext");
            storylandDrawPlayTestText(draw,retailHud,origin,vcs?IM_COL32(244,112,196,255):IM_COL32(210,35,35,255),label.c_str(),height,size.x,"bigtext");
        }
        if(!runtime.playerRestart.controlReleased) {
            if(mission) {
                storylandDrawMissionPresentation(draw,retailHud,*mission,size,camera,&runtime,vcs);
                if(vcs)drawScriptFrameText(draw,retailHud,*mission,size,true);
            }
            return;
        }
    }
    if(!retailCutsceneHudSuppressed && !runtime.playerRestart.active() && runtime.respawnFade>0) {
        draw->AddRectFilled(ImVec2(0,0),size,IM_COL32(0,0,0,int(255*std::clamp(runtime.respawnFade,0.0f,1.0f))));
        return;
    }
    if(!runtime.missionText.empty()) {
        const float width=std::min(680.0f,size.x-80.0f);
        float height=std::clamp(size.y/28.0f,18.0f,30.0f);
        auto extent=storylandMeasurePlayTestText(retailHud,runtime.missionText.c_str(),height,width);
        const bool cinematic=mission&&(mission->widescreen||mission->cutscene.playing);
        const float panelHeight=std::max(0.0f,size.y-cinematicViewportTop)*0.12f;
        const float panelTop=size.y-panelHeight;
        if(cinematic) {
            const float available=std::max(1.0f,panelHeight-8.0f);
            for(unsigned attempt=0;attempt<8&&extent.y>available;++attempt) {
                height*=std::min(0.95f,available/extent.y);
                extent=storylandMeasurePlayTestText(retailHud,runtime.missionText.c_str(),height,width);
            }
            draw->PushClipRect(ImVec2(0,panelTop),size,true);
        }
        const ImVec2 origin(size.x*0.5f,cinematic?panelTop+(panelHeight-extent.y)*0.5f:
            size.y-std::max(28.0f,size.y*0.06f)-extent.y);
        for(const ImVec2 offset:{ImVec2(-1,0),ImVec2(1,0),ImVec2(0,-1),ImVec2(0,1)})
            storylandDrawPlayTestText(draw,retailHud,ImVec2(origin.x+offset.x,origin.y+offset.y),
                IM_COL32(0,0,0,255),runtime.missionText.c_str(),height,width,"gametext",true);
        storylandDrawPlayTestText(draw,retailHud,origin,IM_COL32_WHITE,
            runtime.missionText.c_str(),height,width,"gametext",true);
        if(cinematic)draw->PopClipRect();
    }
    if(mission&&!runtime.pauseMenu)storylandDrawMissionPresentation(draw,retailHud,*mission,size,camera,&runtime,vcs);
    if (retailCutsceneHudSuppressed ||
        (!runtime.pauseMenu && (!runtime.hud || (mission && !mission->wantsHud)))) {
        if (vcs && mission) {
            drawScriptFrameText(draw, retailHud, *mission, size, true);
        }
        return;
    }
    if(!vcs&&mission&&!debugMenuOpen&&!runtime.pauseMenu&&!runtime.paused&&
        !mission->widescreen&&!mission->cutscene.playing)
        storylandDrawLcsNavigationArrow(draw,retailHud,*mission,player,cameraYaw,size);
    if(vcs&&mission&&!debugMenuOpen&&!runtime.pauseMenu&&!runtime.paused) {
        storylandDrawVcsAreaCaption(draw,retailHud,*mission,size);
        storylandDrawVcsVehicleCaption(draw,retailHud,*mission,size);
    }
    const auto text=[&](ImVec2 at,ImU32 color,const char* value) {
        storylandDrawPlayTestText(draw,retailHud,ImVec2(at.x+1,at.y+1),IM_COL32(0,0,0,255),value,ImGui::GetFontSize());
        storylandDrawPlayTestText(draw,retailHud,at,color,value,ImGui::GetFontSize());
    };
    const auto textureFor = [&](const std::string& role)->ImTextureID {
        return storylandRetailSpriteTexture(retailHud,role);
    };
    const auto pcSpriteBound = [&](const std::string& role) {
        if(!retailHud||!retailHud->pcMode)return false;
        const auto current=retailHud->sprites.find(role);
        const auto replacement=retailHud->sprites.find("pc:"+role);
        return current!=retailHud->sprites.end()&&replacement!=retailHud->sprites.end()&&
            current->second&&current->second==replacement->second;
    };
    if(camera&&!debugMenuOpen)
        storylandDrawTargetIndicator(draw,textureFor("triangle16"),textureFor("sitem16"),runtime,pedestrians,*camera,size,
            mission?mission->cameraNearClip:.1f);
    if(!vcs&&mission&&camera&&!runtime.pauseMenu&&!debugMenuOpen) {
        const int32_t bonus=mission->modelIdByName?mission->modelIdByName("bonus"):-1;
        const int32_t clothes=mission->modelIdByName?mission->modelIdByName("clothesp"):-1;
        for(const auto& pickup:mission->pickups) {
            if(pickup.area!=runtime.visibleArea)continue;
            const std::string name=!pickup.modelNameOverride.empty()?pickup.modelNameOverride:
                pickup.model==bonus?"bonus":pickup.model==clothes?"clothesp":"";
            const auto caption=storylandLcsPickupCaption(pickup,name,camera->eye.x,camera->eye.y);
            if(!caption.visible())continue;
            if(!storylandCameraSphereVisible(*camera,{caption.x,caption.y,caption.z},0,size.x/size.y,
                mission->cameraNearClip,1000))continue;
            std::array<float,2> screen{};
            if(!storylandCameraScreenCoordinates(*camera,{caption.x,caption.y,caption.z},size.x/size.y,screen))continue;
            std::string label=caption.text;
            if(!caption.textKey.empty())label=scriptTextUtf8(storylandMissionFormattedText(*mission,caption.textKey));
            if(label.empty())continue;
            const float height=std::clamp(size.y/28.0f,14.0f,28.0f);
            storylandDrawPlayTestText(draw,retailHud,ImVec2(screen[0]*size.x/480.0f,screen[1]*size.y/272.0f),
                IM_COL32(caption.color[0],caption.color[1],caption.color[2],caption.alpha),
                label.c_str(),height,0,"gametext",true);
        }
    }
    const auto sprite = [&](const std::string& role,ImVec2 pos,ImVec2 dimensions,
        ImVec2 uv0=ImVec2(0,0),ImVec2 uv1=ImVec2(1,1),ImU32 tint=IM_COL32_WHITE) {
        const auto texture=textureFor(role);if(!texture)return false;
        draw->AddImage(texture,pos,ImVec2(pos.x+dimensions.x,pos.y+dimensions.y),uv0,uv1,tint);
        return true;
    };
    const bool stadiumRadar=mission&&(vcs?mission->playerInStadium:mission->lcsExtendedWorld.playerInStadium);
    std::vector<const StorylandMissionEnvironment::RadarBlip*> radarBlips;
    if(mission) {
        radarBlips.reserve(mission->radarBlips.size());
        for(const auto& blip:mission->radarBlips)radarBlips.push_back(&blip);
        if(!vcs) {
            const auto pass=[](const auto* blip) {
                using Entity=StorylandMissionEnvironment::BlipEntity;
                const bool coordinate=blip->entity==Entity::Coordinate||blip->entity==Entity::ContactPoint;
                return storylandLcsRadarBlipPass(coordinate,blip->sprite);
            };
            std::stable_sort(radarBlips.begin(),radarBlips.end(),
                [&](const auto* first,const auto* second){return pass(first)<pass(second);});
        }
    }
    if(runtime.pauseMenu) {
        if(!vcs) {
        auto* background=ImGui::GetBackgroundDrawList();
        background->AddRectFilled(ImVec2(0,0),size,IM_COL32(8,8,15,240));
        const auto menuTexture=textureFor("frontend_background");
        if(menuTexture) {
            const ImVec2 dimensions(std::min(520.0f,size.x*0.75f),
                std::min(520.0f,size.y*0.75f));
            const ImVec2 position((size.x-dimensions.x)*0.5f,(size.y-dimensions.y)*0.5f);
            background->AddImage(menuTexture,position,
                ImVec2(position.x+dimensions.x,position.y+dimensions.y),
                ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,180));
        }
        }
        if(runtime.pauseMap&&mission&&retailHud&&retailHud->radarGrid>0&&retailHud->radarTileSize>0) {
            const float viewportHeight=std::max(1.0f,size.y-cinematicViewportTop);
            const float side=std::max(1.0f,vcs?std::min(size.x*.85f,viewportHeight*170.0f/272.0f):
                std::min(size.x-320.0f,size.y-60.0f));
            const ImVec2 top(vcs?(size.x-side)*.5f:290.0f+(size.x-320.0f-side)*.5f,
                vcs?cinematicViewportTop+viewportHeight*45.0f/272.0f:(size.y-side)*.5f);
            const ImVec2 bottom(top.x+side,top.y+side);
            const int grid=retailHud->radarGrid;
            const float minimum=retailHud->radarWorldMinimum;
            const float span=retailHud->radarTileSize*float(grid);
            const float zoom=std::clamp(runtime.pauseMapZoom,1.0f,8.0f);
            const auto project=[&](const StorylandNativePoint& point) {
                return ImVec2(top.x+side*.5f+storylandInvertedMapHorizontal(
                    float((double(point.x)-minimum-span*.5-runtime.pauseMapPanX)/span*side*zoom),invertedMap),
                    top.y+side*.5f-float((double(point.y)-minimum-span*.5-runtime.pauseMapPanY)/span*side*zoom));
            };
            const auto inside=[&](const ImVec2& point) {
                return std::isfinite(point.x)&&std::isfinite(point.y)&&point.x>=top.x&&point.x<=bottom.x&&
                    point.y>=top.y&&point.y<=bottom.y;
            };
            draw->AddRectFilled(top,bottom,stadiumRadar?IM_COL32(204,204,204,255):IM_COL32(20,28,34,255));
            draw->PushClipRect(top,bottom,true);
            if(!stadiumRadar)for(const auto& tile:retailHud->radarTiles) {
                const float width=side/float(grid)*zoom;
                const StorylandNativePoint corner{minimum+float(tile.first%grid)*retailHud->radarTileSize,
                    minimum+float(grid-tile.first/grid)*retailHud->radarTileSize,0};
                const ImVec2 projected=project(corner);
                const ImVec2 cornerOnScreen=invertedMap?ImVec2(projected.x-width,projected.y):projected;
                sprite(tile.second,cornerOnScreen,ImVec2(width,width),
                    invertedMap?ImVec2(1,0):ImVec2(0,0),
                    invertedMap?ImVec2(0,1):ImVec2(1,1));
            }
            const bool pauseWide=size.x/size.y>1.5f;
            const ImVec2 pauseScale(size.x/480.0f,size.y/272.0f);
            for(const auto* item:radarBlips) {
                const auto& blip=*item;
                if(!storylandMissionRadarTraceDisplayed(*mission,blip,vcs,true))continue;
                StorylandNativePoint position;
                if(!storylandMissionBlipPosition(*mission,blip,position,vcs))continue;
                const auto point=project(storylandRadarBlipPoint(*mission,blip,position,true));
                if(!inside(point))continue;
                auto rgba=storylandMissionRadarTint(blip,vcs);
                const ImU32 tint=IM_COL32(rgba[0],rgba[1],rgba[2],rgba[3]);
                const char* name=storylandRadarSpriteName(vcs,blip.sprite);
                const auto nativeHalf=storylandLcsRadarSpriteHalfSize(pauseWide);
                const ImVec2 extent(vcs?12*pauseScale.x:2*nativeHalf[0]*pauseScale.x,
                    vcs?12*pauseScale.y:2*nativeHalf[1]*pauseScale.y);
                const bool drawn=blip.sprite>0&&*name&&sprite(name,
                    ImVec2(point.x-extent.x*.5f,point.y-extent.y*.5f),extent,
                    ImVec2(0,0),ImVec2(1,1),tint);
                if(drawn||blip.sprite>0)continue;
                rgba=storylandMissionBlipColor(blip.color,blip.bright,vcs);
                rgba[3]=255;
                const auto height=storylandRadarHeight(position.z,storylandRadarMapPlayer(*mission).z);
                if(vcs)drawVcsMissionRadarTrace(draw,point,uint8_t(blip.scale),height,rgba,
                    ImVec2(size.x/480,size.y/272),size.x/size.y>1.5f);
                else drawLcsMissionRadarTrace(draw,point,int16_t(blip.scale),height,rgba,pauseScale,pauseWide);
            }
            if(runtime.mapCollectibleLayer>=0&&runtime.mapCollectibleLayer<3) {
                const int layer=runtime.mapCollectibleLayer;
                // Retail MAIN registers completed jumps/rampages/balloons in
                // layers0/1/2; the pause-map selector displays one layer.
                const char* name=storylandVcsRadarSpriteName(48+layer);
                for(const auto& position:mission->toggleableRadarBlips[size_t(layer)]) {
                    const auto point=project({position[0],position[1],0});
                    if(!inside(point))continue;
                    if(!sprite(name,ImVec2(point.x-8,point.y-8),ImVec2(16,16)))
                        draw->AddCircleFilled(point,4,IM_COL32_WHITE,12);
                }
            }
            auto mapPlayer=storylandRadarMapPlayer(*mission);
            if(!vcs&&stadiumRadar) {mapPlayer.x=-1302.530029296875f;mapPlayer.y=1332.8499755859375f;}
            const auto point=project(mapPlayer);
            if(inside(point)&&!vcs) {
                const uint32_t pauseMilliseconds=uint32_t(uint64_t(ImGui::GetTime()*1000.0));
                if(mission->lcsRadarBlink.update(pauseMilliseconds)) {
                    const float worldHeading=heading*.0174532925199433f;
                    const float nativeHeading=std::atan2(-std::cos(worldHeading),std::sin(worldHeading));
                    const unsigned index=storylandLcsPausePlayerSprite(nativeHeading);
                    const char* name=storylandLcsRadarSpriteName(int(index)+54);
                    const float halfWidth=float(int(storylandLcsRadarHorizontal(16.0f,pauseWide)))*pauseScale.x;
                    const float halfHeight=16.0f*pauseScale.y;
                    const ImVec2 origin(point.x-halfWidth,point.y-halfHeight),extent(2*halfWidth,2*halfHeight);
                    const ImVec2 uv0(invertedMap?1.0f:0.0f,0),uv1(invertedMap?0.0f:1.0f,1);
                    sprite(name,ImVec2(origin.x+pauseScale.x,origin.y+pauseScale.y),extent,uv0,uv1,
                        IM_COL32(0,0,0,209));
                    sprite(name,origin,extent,uv0,uv1,IM_COL32(255,255,255,209));
                }
            } else if(inside(point)) {
                const float angle=heading*.0174532925199433f;
                const ImVec2 direction(storylandInvertedMapHorizontal(std::sin(angle),invertedMap),-std::cos(angle));
                draw->AddTriangleFilled(ImVec2(point.x+direction.x*8,point.y+direction.y*8),
                    ImVec2(point.x-direction.x*5+direction.y*5,point.y-direction.y*5-direction.x*5),
                    ImVec2(point.x-direction.x*5-direction.y*5,point.y-direction.y*5+direction.x*5),IM_COL32_WHITE);
            }
            draw->PopClipRect();
            draw->AddRect(top,bottom,IM_COL32(244,112,196,255),0,0,2);
        }
        return;
    }
    if(!runtime.suppressLevelSplash&&!runtime.splashScreen.empty()&&runtime.elapsed-runtime.splashStartedAt<3.5f) {
        std::string splash=lowerHudName(runtime.splashScreen);
        const auto extension=splash.find('.');
        if(extension!=std::string::npos)splash.resize(extension);
        if(sprite("splash:"+splash,ImVec2(0,0),size))return;
    }
    if(!runtime.missionHelp.empty()) {
        const float width=std::min(430.0f,size.x-44.0f);
        StorylandTextLayout helpLayout;
        if(helpBindings&&helpBindings->keyboard)helpLayout.pcHelpBindings=helpBindings;
        const auto measured=storylandMeasurePlayTestText(retailHud,runtime.missionHelp.c_str(),17,width-20,"gametext",&helpLayout);
        draw->AddRectFilled(ImVec2(22,25),ImVec2(22+width,45+measured.y),IM_COL32(0,0,0,190),3);
        storylandDrawPlayTestText(draw,retailHud,ImVec2(32,35),IM_COL32_WHITE,
            runtime.missionHelp.c_str(),17,width-20,"gametext",false,&helpLayout);
    }
    if(vcs&&mission&&retailHud&&mission->pager.displayed()&&
        std::isfinite(mission->pager.displayOffset)) {
        const auto texture=storylandRetailSpriteTexture(retailHud,"hud:pager");
        if(texture) {
            const bool widescreen=size.x/size.y>1.5f;
            const ImVec2 scale(size.x/480.0f,size.y/272.0f);
            const float offset=mission->pager.displayOffset;
            const auto point=[&](float x,float y) {return ImVec2(x*scale.x,y*scale.y);};
            draw->AddImage(texture,point(26.0f-offset,18.66666603088379f),
                point((widescreen?122.0f:154.0f)-offset,63.578941345214844f),
                ImVec2(.001f,0),ImVec2(1,.999f),IM_COL32_WHITE);
            const auto font=retailHud->fonts.find("pagertext");
            if(font!=retailHud->fonts.end()) {
                std::u16string text;
                const auto* cursor=reinterpret_cast<const unsigned char*>(runtime.pagerText.c_str());
                while(*cursor) {
                    const uint32_t code=readRetailUtf8(cursor);
                    if(code<=0xFFFFu)text.push_back(char16_t(code));
                    else {
                        text.push_back(char16_t(0xD800u+((code-0x10000u)>>10)));
                        text.push_back(char16_t(0xDC00u+((code-0x10000u)&0x3FFu)));
                    }
                }
                const ImVec4 clip((widescreen?34.0f:38.0f)-offset,28,
                    (widescreen?113.0f:142.0f)-offset,56);
                const auto layout=storylandLayoutScriptText(font->second,text,.4f,widescreen,true,
                    int32_t(34.0f-offset),28,clip,0,IM_COL32(50,50,50,255),false,false,true);
                draw->PushClipRect(point((widescreen?34.0f:38.0f)-offset,28),
                    point((widescreen?113.0f:142.0f)-offset,56),true);
                drawScriptTextLayout(draw,retailHud,"fsfonts:pagertext",layout,scale,{});
                draw->PopClipRect();
            }
        }
    }
    // Gameplay HUD stays anchored to the screen, even when the Play Test
    // controls are open. Sliding it left put the money/bars beneath the panel.
    (void)debugMenuOpen;
    const auto componentVisible = [&](uint8_t component) {
        return !vcs || !mission || storylandVcsHudComponentVisible(*mission,component);
    };
    const auto cluster=storylandHudClusterLayout(size.x,size.y,runtime.armour>0,cinematicViewportTop);
    // HOOK_WIDESCREEN_MODE is native preference14; 2AF658 reads that flag.
    const bool nativeWide=size.y>cinematicViewportTop&&size.x/(size.y-cinematicViewportTop)>1.5f;
    const auto vcsHud=storylandVcsHudLayout(size.x,size.y,cinematicViewportTop,nativeWide);
    const auto lcsHud=storylandLcsHudLayout(size.x,size.y,cinematicViewportTop,nativeWide);
    const uint32_t hudFrame=mission?mission->nativeFrameCounter:runtime.trafficClockMilliseconds*30u/1000u;
    const uint32_t hudTime=mission?mission->gameMilliseconds:runtime.trafficClockMilliseconds;
    const auto numberUv=[&](unsigned char code,std::array<float,4>& uv) {
        if(!pcSpriteBound("hudnumbers"))return false;
        const auto image=retailHud->sprites.find("hudnumbers");
        const auto font=retailHud->fonts.find("hudnumbers");
        if(image==retailHud->sprites.end()||!image->second||font==retailHud->fonts.end())return false;
        const auto* glyph=storylandRetailTextGlyph(font->second,code==0x16?'*':code);
        if(!glyph||!glyph->atlasWidth||!glyph->atlasHeight)return false;
        const float width=float(image->second->width),height=float(image->second->height);
        uv={(glyph->u+.5f)/width,(glyph->v+.5f)/height,
            (glyph->u+glyph->atlasWidth-.5f)/width,(glyph->v+glyph->atlasHeight-.5f)/height};
        return true;
    };
    const auto vcsNumericText=[&](float nativeX,float nativeY,ImU32 colour,
        const char* value,const char* fontName) {
        const uint8_t alpha=uint8_t(((colour>>IM_COL32_A_SHIFT)&255u)*209u/255u);
        const bool money=std::strcmp(fontName,"hudmoney")==0;
        const auto glyphs=storylandVcsHudNumbers(value,nativeX,nativeY,money,nativeWide,alpha);
        size_t character=0;
        for(const auto& glyph:glyphs) {
            const auto& rectangle=glyph.rectangle;
            auto uv=glyph.uv;
            const auto code=static_cast<unsigned char>(value[character++]);
            const bool monochrome=numberUv(code,uv);
            auto rgba=glyph.color;
            // Native hudnumbers contains these ink colors; the PC atlas is white.
            if(monochrome&&code!='*')rgba=money?std::array<uint8_t,4>{32,110,32,alpha}:
                std::array<uint8_t,4>{193,140,44,alpha};
            sprite("hudnumbers",ImVec2(vcsHud.x(rectangle[0]),vcsHud.y(rectangle[1])),
                ImVec2(vcsHud.x(rectangle[2]-rectangle[0]),(rectangle[3]-rectangle[1])*vcsHud.scaleY),
                ImVec2(uv[0],uv[1]),ImVec2(uv[2],uv[3]),IM_COL32(rgba[0],rgba[1],rgba[2],rgba[3]));
        }
    };
    const auto lcsNumericText=[&](float x,float y,const char* value,bool money=false) {
        size_t character=0;
        for(const auto& glyph:storylandLcsHudNumbers(value,x,y,money,nativeWide)) {
            const auto& rectangle=glyph.rectangle;
            auto uv=glyph.uv;
            numberUv(static_cast<unsigned char>(value[character++]),uv);
            const auto& color=glyph.color;
            sprite("hudnumbers",ImVec2(lcsHud.x(rectangle[0]),lcsHud.y(rectangle[1])),
                ImVec2(lcsHud.x(rectangle[2]-rectangle[0]),(rectangle[3]-rectangle[1])*lcsHud.scaleY),
                ImVec2(uv[0],uv[1]),ImVec2(uv[2],uv[3]),IM_COL32(color[0],color[1],color[2],color[3]));
        }
    };
    const auto lcsFontText=[&](const std::string& value,float x,float y,float scaleX,float scaleY,
        StorylandLcsHudTextAlignment alignment,bool shadow,uint8_t alpha=209) {
        if(!retailHud)return;
        const auto font=retailHud->fonts.find("gametext");
        const auto image=retailHud->sprites.find("gametext");
        if(font==retailHud->fonts.end()||image==retailHud->sprites.end()||!image->second)return;
        const auto glyphs=storylandLcsHudFontText(font->second,value,x,y,scaleX,scaleY,alignment,
            image->second->width,image->second->height);
        for(int pass=shadow?0:1;pass<2;++pass)for(const auto& glyph:glyphs) {
            const float offset=pass?0.0f:1.0f;
            const auto& rectangle=glyph.rectangle;const auto& uv=glyph.uv;
            sprite("gametext",ImVec2(lcsHud.x(rectangle[0]+offset),lcsHud.y(rectangle[1]+offset)),
                ImVec2(lcsHud.x(rectangle[2]-rectangle[0]),(rectangle[3]-rectangle[1])*lcsHud.scaleY),
                ImVec2(uv[0],uv[1]),ImVec2(uv[2],uv[3]),
                pass?IM_COL32(255,255,255,alpha):IM_COL32(0,0,0,alpha));
        }
    };
    char hudValue[64];
    const auto clock=storylandClockParts(runtime.clockHour);
    std::snprintf(hudValue,sizeof(hudValue),"%02d:%02d",
        int(clock.hour),int(clock.minute));
    if(componentVisible(1)&&(!mission||(vcs?storylandVcsClockEventVisible(*mission,runtime.clockHour):
        storylandLcsClockEventVisible(*mission,runtime.clockHour)))) {
        if(vcs)vcsNumericText(vcsHud.moneyLeft(),17.0f,IM_COL32_WHITE,hudValue,"hudnumbers");
        else lcsNumericText(lcsHud.numbersLeft(),17.0f,hudValue);
    }
    if(vcs&&componentVisible(0)) {
        const auto* weapon=runtime.equippedWeapon<runtime.weapons.size()?
            &runtime.weapons[runtime.equippedWeapon]:nullptr;
        const auto* inventory=runtime.equippedWeapon<runtime.inventory.size()?
            &runtime.inventory[runtime.equippedWeapon]:nullptr;
        const auto native=storylandVcsWeaponHud(weapon?std::max(0,weapon->nativeId):0,
            weapon?weapon->slot:0,weapon?weapon->clipSize:0,
            inventory?inventory->ammunition:0,inventory?inventory->magazine:0,nativeWide,
            true,(!mission||mission->vcsAmmoHudVisible)&&inventory,runtime.killFrenzy.active());
        const auto& rectangle=native.icon;
        const auto tint=IM_COL32(255,255,255,native.alpha);
        sprite(storylandVcsWeaponIconKey(native.nativeWeapon),
            ImVec2(vcsHud.x(rectangle.left),vcsHud.y(rectangle.top)),
            ImVec2(vcsHud.x(rectangle.right-rectangle.left),
                (rectangle.bottom-rectangle.top)*vcsHud.scaleY),ImVec2(0,0),ImVec2(1,1),tint);
        if(retailHud) {
            const auto font=retailHud->fonts.find("pagertext");
            if(font!=retailHud->fonts.end())for(size_t index=0;index<native.textCount;++index) {
                const auto& part=native.ammunition[index];
                const auto& box=part.clip;
                const ImVec4 clip(box.left,box.top,box.right,box.bottom);
                const std::u16string text(part.text.begin(),part.text.end());
                const auto layout=storylandLayoutScriptText(font->second,text,native.fontScale,
                    nativeWide,true,0,0,clip,part.alignment,tint);
                draw->PushClipRect(ImVec2(vcsHud.x(box.left),vcsHud.y(box.top)),
                    ImVec2(vcsHud.x(box.right),vcsHud.y(box.bottom)),true);
                drawScriptTextLayout(draw,retailHud,"pagertext",layout,
                    ImVec2(vcsHud.scaleX,vcsHud.scaleY),
                    ImVec2(0,vcsHud.scaleY>0?vcsHud.top/vcsHud.scaleY:0));
                draw->PopClipRect();
            }
        }
    } else if(!vcs&&componentVisible(0)) {
        const auto* weapon=runtime.equippedWeapon<runtime.weapons.size()?
            &runtime.weapons[runtime.equippedWeapon]:nullptr;
        const auto* inventory=runtime.equippedWeapon<runtime.inventory.size()?
            &runtime.inventory[runtime.equippedWeapon]:nullptr;
        const auto native=storylandLcsWeaponHud(weapon?std::max(0,weapon->nativeId):0,
            weapon?weapon->slot:0,weapon?weapon->clipSize:0,
            inventory?inventory->ammunition:0,inventory?inventory->magazine:0,
            nativeWide,inventory!=nullptr,runtime.killFrenzy.active());
        const auto& rectangle=native.icon;
        sprite(storylandLcsWeaponIconKey(native.nativeWeapon),
            ImVec2(lcsHud.x(rectangle[0]),lcsHud.y(rectangle[1])),
            ImVec2(lcsHud.x(rectangle[2]-rectangle[0]),(rectangle[3]-rectangle[1])*lcsHud.scaleY),
            ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,native.alpha));
        for(size_t index=0;index<native.textCount;++index) {
            const auto& part=native.ammunition[index];
            lcsFontText(part.text,part.x,part.y,native.scaleX,native.scaleY,part.alignment,true,native.alpha);
        }
    }
    const auto visibleHudItem=[&](int16_t item) {
        return vcs||!mission||storylandLcsHudItemVisible(mission->lcsHudFlashItem,item,hudFrame);
    };
    const bool healthVisible=componentVisible(4)&&visibleHudItem(4)&&
        (!vcs||!mission||storylandHudObjectVisible(*mission,4,runtime.health));
    const bool armourVisible=componentVisible(3)&&visibleHudItem(3)&&
        (!vcs||!mission||storylandHudObjectVisible(*mission,3,runtime.armour));
    if(vcs) {
        const auto vcsBar=[&](float level,float maximum,bool armour) {
            const auto bar=vcsHud.bar(level,maximum,armour);
            const float height=bar.bottom-bar.top;
            const char* fill=armour?"bar_inside1":"bar_inside2";
            const char* empty=armour?"bar_inside1dark":"bar_inside2dark";
            const ImU32 tint=IM_COL32(255,255,255,209);
            if(bar.fillRight>bar.left&&!sprite(fill,ImVec2(bar.left,bar.top),
                ImVec2(bar.fillRight-bar.left,height),ImVec2(0,0),ImVec2(bar.fillU,1),tint))
                draw->AddRectFilled(ImVec2(bar.left,bar.top),ImVec2(bar.fillRight,bar.bottom),
                    armour?IM_COL32(180,200,235,255):IM_COL32(244,112,196,255));
            if(bar.right>bar.fillRight&&!sprite(empty,ImVec2(bar.fillRight,bar.top),
                ImVec2(bar.right-bar.fillRight,height),ImVec2(bar.fillU,0),ImVec2(1,1),tint))
                draw->AddRectFilled(ImVec2(bar.fillRight,bar.top),ImVec2(bar.right,bar.bottom),
                    IM_COL32(25,20,27,220));
            if(!sprite("bar_outline",ImVec2(bar.left,bar.top),ImVec2(bar.right-bar.left,height),
                ImVec2(0,0),ImVec2(1,1),tint))
                draw->AddRect(ImVec2(bar.left,bar.top),ImVec2(bar.right,bar.bottom),IM_COL32_WHITE);
        };
        if(healthVisible)vcsBar(runtime.health,runtime.maximumHealth(),false);
        bool reserveVisible=false;
        const auto vcsMeter=[&](float fraction,const StorylandVcsHudMeter& meter,bool oxygen) {
            reserveVisible=true;
            if(meter.alpha==0)return;
            const auto bar=vcsHud.meter(fraction);
            const float height=bar.bottom-bar.top;
            const uint8_t alpha=uint8_t(unsigned(meter.alpha)*209u/255u);
            const ImU32 tint=oxygen?IM_COL32(31,31,127,alpha):IM_COL32(31,127,31,alpha);
            if(bar.fillRight>bar.left&&!sprite("bar_inside1",ImVec2(bar.left,bar.top),
                ImVec2(bar.fillRight-bar.left,height),ImVec2(0,0),ImVec2(bar.fillU,1),tint))
                draw->AddRectFilled(ImVec2(bar.left,bar.top),ImVec2(bar.fillRight,bar.bottom),tint);
            if(bar.right>bar.fillRight&&!sprite("bar_inside1dark",ImVec2(bar.fillRight,bar.top),
                ImVec2(bar.right-bar.fillRight,height),ImVec2(bar.fillU,0),ImVec2(1,1),tint))
                draw->AddRectFilled(ImVec2(bar.fillRight,bar.top),ImVec2(bar.right,bar.bottom),
                    IM_COL32(8,8,16,alpha));
            if(!sprite("bar_outline",ImVec2(bar.left,bar.top),ImVec2(bar.right-bar.left,height),
                ImVec2(0,0),ImVec2(1,1),tint))
                draw->AddRect(ImVec2(bar.left,bar.top),ImVec2(bar.right,bar.bottom),tint);
        };
        if(mission) {
            auto& meters=mission->vcsHudMeters;
            if(!runtime.paused&&!runtime.pauseMenu)
                meters.advance(runtime.playerWater.active,mission->nativeScriptTimeStep,
                    mission->nativeFrameCounter);
            if(meters.oxygenVisible())vcsMeter(runtime.playerWater.air/30.0f,meters.oxygen,true);
            if(meters.sprintVisible())vcsMeter(mission->playerSprintMaximum>0?
                mission->playerSprintEnergy/mission->playerSprintMaximum:0,meters.sprint,false);
        } else if(runtime.playerWater.active) {
            StorylandVcsHudMeter meter;
            meter.alpha=255;
            meter.phase=StorylandVcsHudMeter::Phase::Visible;
            vcsMeter(runtime.playerWater.air/30.0f,meter,true);
        }
        if(!reserveVisible&&runtime.armour>1.0f&&armourVisible)
            vcsBar(runtime.armour,runtime.maximumArmour(),true);
        std::snprintf(hudValue,sizeof(hudValue),"$%08d",runtime.money);
        if(componentVisible(2))vcsNumericText(vcsHud.moneyLeft(),
            mission&&mission->vcsCashHudRaised?16.0f:53.0f,IM_COL32_WHITE,hudValue,"hudmoney");
    } else {
        const auto lcsBar=[&](float level,float maximum,bool armour) {
            const auto bar=lcsHud.bar(level,maximum,armour);
            const float height=bar.bottom-bar.top;
            const ImU32 tint=IM_COL32(255,255,255,209);
            if(bar.fillRight>bar.left)
                sprite(armour?"bar_inside1":"bar_inside2",ImVec2(bar.left,bar.top),
                    ImVec2(bar.fillRight-bar.left,height),ImVec2(0,0),ImVec2(bar.fillU,1),tint);
            if(bar.right>bar.fillRight)
                sprite(armour?"bar_inside1dark":"bar_inside2dark",ImVec2(bar.fillRight,bar.top),
                    ImVec2(bar.right-bar.fillRight,height),ImVec2(bar.fillU,0),ImVec2(1,1),tint);
            sprite("bar_outline",ImVec2(bar.left,bar.top),ImVec2(bar.right-bar.left,height),
                ImVec2(.01f,0),ImVec2(1,1),tint);
            const auto upgrade=lcsHud.upgrade(maximum,armour);
            if(upgrade.visible)lcsFontText("+",upgrade.x,upgrade.y,upgrade.scaleX,upgrade.scaleY,
                StorylandLcsHudTextAlignment::Center,false);
        };
        const int16_t flashing=mission?mission->lcsHudFlashItem:-1;
        const bool healthDrawn=healthVisible&&storylandLcsHealthVisible(runtime.health,flashing,hudFrame);
        const bool armourDrawn=armourVisible&&storylandLcsArmourVisible(runtime.armour,flashing,hudFrame,
            runtime.lcsHud.armourLostAt,hudTime);
        if(healthDrawn)
            lcsBar(runtime.health,runtime.maximumHealth(),false);
        if(armourDrawn)
            lcsBar(runtime.armour,runtime.maximumArmour(),true);
        if(mission&&!runtime.paused&&!runtime.pauseMenu)
            mission->lcsHudFlash.advance(mission->lcsHudFlashItem,runtime.health,runtime.armour,
                healthDrawn,armourDrawn,hudFrame);
        const int visibleMoney=runtime.lcsHud.moneyInitialized?runtime.lcsHud.visibleMoney:runtime.money;
        std::snprintf(hudValue,sizeof(hudValue),"$%08d",visibleMoney);
        if(componentVisible(2))lcsNumericText(lcsHud.numbersLeft(),53.0f,hudValue,true);
    }
    const int suspendedWanted=!vcs&&mission?mission->lcsExtendedWorld.minSuspendedWanted:0;
    const int displayedWanted=std::max(runtime.wanted,suspendedWanted);
    if (componentVisible(5) && displayedWanted > 0) {
        for(int star=0;star<std::min(displayedWanted,6);++star) {
            if(vcs&&mission&&!storylandVcsActiveWantedStarVisible(
                runtime.crime.vcsWantedChangeKnown,runtime.crime.vcsWantedChangedAt,
                runtime.crime.milliseconds,mission->nativeFrameCounter))continue;
            if(!vcs) {
                if(storylandLcsWantedStarVisible(star,runtime.wanted,suspendedWanted,
                    runtime.crime.vcsWantedChangeKnown,runtime.crime.vcsWantedChangedAt,
                    runtime.crime.milliseconds,hudFrame))
                    lcsNumericText(441.0f-float(star)*lcsHud.wantedStep(),63.0f,"*",star>=runtime.wanted);
                continue;
            }
            const ImVec2 pos(vcsHud.x(441.0f-float(star)*vcsHud.wantedStep()),vcsHud.y(63.0f));
            const int alpha=255;
            if(vcs&&retailHud&&retailHud->fonts.count("hudnumbers"))
                vcsNumericText(441.0f-float(star)*vcsHud.wantedStep(),63.0f,
                    IM_COL32(241,170,57,alpha),"*","hudnumbers");
            else if(!sprite("star",pos,ImVec2(22*cluster.scale,22*cluster.scale),ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,alpha)))
                text(pos,IM_COL32(255,230,112,alpha),"*");
        }
    }
    if(!vcs&&mission&&runtime.killFrenzy.active()&&!debugMenuOpen&&!runtime.pauseMenu) {
        const float scale=size.y/448.0f;
        const auto counter=[&](float y,ImU32 color,const char* value) {
            const auto extent=storylandMeasurePlayTestText(retailHud,value,24*scale,0,"hudnumbers");
            const ImVec2 origin(size.x-34*scale-extent.x,y*scale);
            storylandDrawPlayTestText(draw,retailHud,ImVec2(origin.x+scale,origin.y+scale),
                IM_COL32_BLACK,value,24*scale,0,"hudnumbers");
            storylandDrawPlayTestText(draw,retailHud,origin,color,value,24*scale,0,"hudnumbers");
        };
        char value[32]{};
        const int32_t remaining=storylandKillFrenzyRemainingMilliseconds(runtime);
        if(remaining>=0&&(remaining>4000||(mission->nativeFrameCounter&1u))) {
            std::snprintf(value,sizeof(value),"%u:%02u",unsigned(remaining/60000),unsigned(remaining%60000/1000));
            counter(108,IM_COL32(0,207,133,255),value);
        }
        std::snprintf(value,sizeof(value),"%d",std::max(0,runtime.killFrenzy.targetKills-runtime.killFrenzy.kills));
        counter(143,IM_COL32(156,91,40,255),value);
        const auto& frenzy=runtime.killFrenzy;
        const uint32_t elapsed=mission->gameMilliseconds-frenzy.startedAt;
        const uint32_t begin=frenzy.completedText?3000u:0u,end=frenzy.completedText?11000u:8000u;
        if(frenzy.text!="PAGE_00"&&!frenzy.text.empty()&&elapsed>=begin&&elapsed<=end) {
            const uint32_t fade=std::min({500u,elapsed-begin,end-elapsed});
            const auto message=scriptTextUtf8(storylandMissionFormattedText(*mission,frenzy.text));
            storylandDrawPlayTestText(draw,retailHud,ImVec2(size.x*.5f,size.y*.5f),
                IM_COL32(255,255,128,int(fade*255u/500u)),message.c_str(),24*scale,size.x-30*scale,"gametext",true);
        }
    }
    if(vcs&&!debugMenuOpen&&!runtime.pauseMenu&&!runtime.paused&&
        (!mission||(!mission->widescreen&&!mission->cutscene.playing))) {
        const float scale=std::clamp(size.y/720.0f,.6f,1.5f),height=18.0f*scale;
        float top=std::max(214.0f,size.y*.30f);
        const auto status=[&](const char* value,ImU32 color) {
            const auto extent=storylandMeasurePlayTestText(retailHud,value,height,0,"gametext");
            const ImVec2 origin(size.x-28.0f-extent.x,top);
            storylandDrawPlayTestText(draw,retailHud,ImVec2(origin.x+scale,origin.y+scale),
                IM_COL32_BLACK,value,height,0,"gametext");
            storylandDrawPlayTestText(draw,retailHud,origin,color,value,height,0,"gametext");
            top+=height*1.5f;
        };
        char value[96]{};
        const auto adrenaline=storylandAdrenalineRemainingMilliseconds(runtime);
        if(adrenaline) {
            std::snprintf(value,sizeof(value),"Adrenaline: %u s",unsigned((adrenaline+999u)/1000u));
            status(value,accent);
        }
        if(runtime.killFrenzy.active()) {
            const auto& frenzy=runtime.killFrenzy;
            std::snprintf(value,sizeof(value),"%s: %d / %d",
                frenzy.onlyHeadshots?"Headshot kills":"Rampage kills",frenzy.kills,frenzy.targetKills);
            status(value,accent);
            const int32_t remaining=storylandKillFrenzyRemainingMilliseconds(runtime);
            if(remaining>=0) {
                const uint32_t seconds=(uint32_t(remaining)+999u)/1000u;
                std::snprintf(value,sizeof(value),"Time: %u:%02u",unsigned(seconds/60u),unsigned(seconds%60u));
                status(value,remaining<=10000?IM_COL32(255,170,110,255):IM_COL32_WHITE);
            }
        }
    }
    const auto nativeRadar=storylandVcsRadarLayout(nativeWide);
    const auto lcsRadar=storylandLcsRadarLayout(nativeWide);
    const ImVec2 center=vcs?ImVec2(vcsHud.x(nativeRadar.centerX()),vcsHud.y(nativeRadar.centerY())):
        ImVec2(lcsHud.x(lcsRadar.centerX()),lcsHud.y(lcsRadar.centerY()));
    const ImVec2 radarRadius=vcs?ImVec2(vcsHud.x(nativeRadar.width*.5f),nativeRadar.height*.5f*vcsHud.scaleY):
        ImVec2(lcsRadar.width*.5f*lcsHud.scaleX,lcsRadar.height*.5f*lcsHud.scaleY);
    constexpr float radius=78,radians=0.01745329251994329577f;
    const bool radarInVehicle=runtime.occupiedVehicle>=0&&size_t(runtime.occupiedVehicle)<runtime.vehicles.size();
    float radarSpeed=0;
    if(radarInVehicle) {
        const auto& vehicle=runtime.vehicles[size_t(runtime.occupiedVehicle)];
        radarSpeed=std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed);
    }
    const float range=vcs?storylandNativeRadarRange(radarInVehicle,radarSpeed,
        mission&&mission->radarPositionFrozen,mission?mission->radarRange:0):
        storylandLcsRadarRange(radarInVehicle,radarSpeed);
    const StorylandNativePoint radarOrigin=vcs&&mission&&mission->radarPositionFrozen?
        StorylandNativePoint{mission->radarPosition[0],mission->radarPosition[1],mission->radarPosition[2]}:player;
    const bool radarVisible=visibleHudItem(8)&&(!mission||(mission->radarVisible&&mission->radarDisplayMode!=(vcs?3:2)&&
        (!vcs||storylandHudObjectVisible(*mission,8))));
    if(radarVisible) {
    const ImVec2 radarTop(center.x-radarRadius.x,center.y-radarRadius.y);
    const ImVec2 radarSize(radarRadius.x*2.0f,radarRadius.y*2.0f);
    if(vcs) {
        draw->AddEllipseFilled(center,radarRadius,IM_COL32(0,0,0,245),0,96);
        const bool retailRadarBackdrop=sprite("radardiscback",radarTop,radarSize,
            ImVec2(5.0f/64,5.0f/64),ImVec2(59.0f/64,59.0f/64));
        if(!retailRadarBackdrop)draw->AddEllipseFilled(center,radarRadius,IM_COL32(22,28,35,225),0,64);
    }
    if(stadiumRadar&&mission->radarDisplayMode!=1)
        draw->AddEllipseFilled(center,vcs?ImVec2(radarRadius.x-2,radarRadius.y-2):radarRadius,
            IM_COL32(204,204,204,vcs?255:209),0,vcs?96:24);
    const float angle=cameraYaw*radians;
    const auto radar=[&](StorylandNativePoint position) {
        const float dx=position.x-radarOrigin.x,dy=position.y-radarOrigin.y;
        return ImVec2(center.x+storylandInvertedMapHorizontal(
            (std::sin(angle)*dx-std::cos(angle)*dy)*radarRadius.x/range,invertedMap),
            center.y-(std::cos(angle)*dx+std::sin(angle)*dy)*radarRadius.y/range);
    };
    const auto inside=[&](ImVec2 point) {return std::hypot((point.x-center.x)/radarRadius.x,
        (point.y-center.y)/radarRadius.y)<1-3/radius;};
    (void)paths;
    if(retailHud&&!stadiumRadar&&(!mission||mission->radarDisplayMode!=1))for(const auto& tile:retailHud->radarTiles) {
        const int grid=retailHud->radarGrid;
        const float tileSize=retailHud->radarTileSize;
        const float west=retailHud->radarWorldMinimum+float(tile.first%grid)*tileSize;
        const float north=retailHud->radarWorldMinimum+float(grid-tile.first/grid)*tileSize;
        if(radarOrigin.x+range<west||radarOrigin.x-range>west+tileSize||radarOrigin.y+range<north-tileSize||radarOrigin.y-range>north)continue;
        const auto a=radar({west,north,0}),b=radar({west+tileSize,north,0});
        const auto c=radar({west+tileSize,north-tileSize,0}),d=radar({west,north-tileSize,0});
        const std::array<StorylandRadarVertex,4> corners{{
            {(a.x-center.x)/radarRadius.x,(a.y-center.y)/radarRadius.y,0,0},
            {(b.x-center.x)/radarRadius.x,(b.y-center.y)/radarRadius.y,1,0},
            {(c.x-center.x)/radarRadius.x,(c.y-center.y)/radarRadius.y,1,1},
            {(d.x-center.x)/radarRadius.x,(d.y-center.y)/radarRadius.y,0,1}}};
        auto polygon=storylandClipRadarTile(corners,0,0,vcs?1-2/radius:1.0f,vcs?48:24);
        for(auto& vertex:polygon) {
            vertex.x=center.x+vertex.x*radarRadius.x;
            vertex.y=center.y+vertex.y*radarRadius.y;
        }
        if(polygon.size()<3)continue;
        const auto texture=textureFor(tile.second);if(!texture)continue;
        for(size_t vertex=1;vertex+1<polygon.size();++vertex) {
            const auto& first=polygon[0];const auto& second=polygon[vertex];const auto& third=polygon[vertex+1];
            draw->AddImageQuad(texture,ImVec2(first.x,first.y),ImVec2(second.x,second.y),ImVec2(third.x,third.y),ImVec2(third.x,third.y),
                ImVec2(first.u,first.v),ImVec2(second.u,second.v),ImVec2(third.u,third.v),ImVec2(third.u,third.v),
                IM_COL32(255,255,255,vcs?255:209));
        }
    }
    if(vcs&&radarOverlay)radarOverlay(draw,center,radarRadius,radarOrigin,cameraYaw,range,invertedMap);
    const bool pcRadarFrame=pcSpriteBound("radardisc");
    if(pcRadarFrame&&(vcs||!mission||mission->radarDisplayMode!=1)) {
        // The supplied PC image is the upper-left quarter, with four pixels of outer padding.
        constexpr float outer=4.0f/64.0f;
        for(unsigned row=0;row<2;++row)for(unsigned column=0;column<2;++column) {
            const ImVec2 position(column?center.x:radarTop.x,row?center.y:radarTop.y);
            sprite("radardisc",position,radarRadius,
                ImVec2(column?1.0f:outer,row?1.0f:outer),
                ImVec2(column?outer:1.0f,row?outer:1.0f));
        }
    }
    else if(vcs)sprite("radardisc",radarTop,radarSize,ImVec2(2.0f/64,2.0f/64),ImVec2(62.0f/64,62.0f/64));
    else if(!mission||mission->radarDisplayMode!=1)
        sprite("radardisc",ImVec2(lcsHud.x(lcsRadar.left-4),lcsHud.y(lcsRadar.top-4)),
            ImVec2((lcsRadar.width+8)*lcsHud.scaleX,(lcsRadar.height+8)*lcsHud.scaleY));
    if(!vcs) {
        const auto texture=textureFor("radar_centre");
        if(texture) {
            const auto corners=storylandLcsRotatingRadarSprite(0,0,
                (heading-cameraYaw)*radians-3.1415927410125732f,nativeWide);
            std::array<ImVec2,4> points;
            for(size_t index=0;index<points.size();++index)
                points[index]=ImVec2(center.x+storylandInvertedMapHorizontal(corners[index][0],invertedMap)*lcsHud.scaleX,
                    center.y+corners[index][1]*lcsHud.scaleY);
            draw->AddImageQuad(texture,points[0],points[1],points[2],points[3],
                ImVec2(0,0),ImVec2(1,0),ImVec2(1,1),ImVec2(0,1),IM_COL32(255,255,255,209));
        }
        const auto north=storylandProjectMissionBlip({radarOrigin.x,radarOrigin.y+range*1.4142135381698608f,
            radarOrigin.z},radarOrigin,cameraYaw,range,false);
        const auto half=storylandLcsRadarSpriteHalfSize(nativeWide);
        const ImVec2 extent(2*half[0]*lcsHud.scaleX,2*half[1]*lcsHud.scaleY);
        const ImVec2 point(center.x+storylandInvertedMapHorizontal(north.x,invertedMap)*radarRadius.x,
            center.y+north.y*radarRadius.y);
        sprite("radar_north",ImVec2(point.x-extent.x*.5f,point.y-extent.y*.5f),extent,
            ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,209));
    }
    if(!mission||mission->radarDisplayMode!=2) {
    if(vcs) {
    for(const auto& actor:pedestrians) {const auto point=radar(actor.position);if(inside(point)) draw->AddCircleFilled(point,2,IM_COL32_WHITE);}
    for(const auto& actor:cars) {
        const auto point=radar(actor.position);
        if(!inside(point)) continue;
        draw->AddRectFilled(ImVec2(point.x-2,point.y-2),ImVec2(point.x+2,point.y+2),IM_COL32(255,105,190,255));
    }
    if(runtime.pickupsEnabled) for(const auto& pickup:runtime.pickups) {
        if(pickup.respawn>0) continue;
        const auto point=radar(pickup.position);
        if(inside(point)) draw->AddCircleFilled(point,3,IM_COL32(80,245,210,255));
    }
    }
    for(const auto* item:radarBlips) {
        const auto& blip=*item;
        if(!storylandMissionRadarTraceDisplayed(*mission,blip,vcs,false))continue;
        StorylandNativePoint position;
        if(!storylandMissionBlipPosition(*mission,blip,position,vcs))continue;
        position = storylandRadarBlipPoint(*mission, blip, position, false);
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
            continue;
        }
        const auto projected=storylandProjectMissionBlip(position,radarOrigin,cameraYaw,range,blip.shortRange);
        if(!projected.visible)continue;
        const ImVec2 point(center.x+storylandInvertedMapHorizontal(projected.x*radarRadius.x,invertedMap),
            center.y+projected.y*radarRadius.y);
        const uint8_t distanceAlpha=vcs?storylandVcsRadarDistanceAlpha(projected.distance):
            storylandLcsRadarDistanceAlpha(projected.distance);
        auto rgba=storylandMissionRadarTint(blip,vcs,
            storylandRadarSpriteAlpha(position,radarOrigin,distanceAlpha));
        const ImU32 tint=IM_COL32(rgba[0],rgba[1],rgba[2],rgba[3]);
        const char* textureName=storylandRadarSpriteName(vcs,blip.sprite);
        const auto half=storylandLcsRadarSpriteHalfSize(nativeWide);
        const ImVec2 extent=vcs?ImVec2(vcsHud.x(12),12*vcsHud.scaleY):
            ImVec2(2*half[0]*lcsHud.scaleX,2*half[1]*lcsHud.scaleY);
        const bool drawn=blip.sprite>0&&*textureName&&sprite(textureName,
            ImVec2(point.x-extent.x*.5f,point.y-extent.y*.5f),extent,ImVec2(0,0),ImVec2(1,1),tint);
        if(drawn||blip.sprite>0)continue;
        rgba=storylandMissionBlipColor(blip.color,blip.bright,vcs);
        rgba[3]=255;
        const auto height=storylandRadarHeight(position.z,player.z);
        if(vcs)drawVcsMissionRadarTrace(draw,point,uint8_t(blip.scale),height,rgba,
            ImVec2(vcsHud.scaleX,vcsHud.scaleY),nativeWide);
        else drawLcsMissionRadarTrace(draw,point,int16_t(blip.scale),height,rgba,
            ImVec2(lcsHud.scaleX,lcsHud.scaleY),nativeWide);
    }
    }
    if(vcs) {
    const float rotation=(heading-cameraYaw)*radians;
    const ImVec2 direction(storylandInvertedMapHorizontal(-std::sin(rotation),invertedMap),-std::cos(rotation));
    if(!sprite("radar_centre",ImVec2(center.x-9,center.y-9),ImVec2(18,18)))
        draw->AddTriangleFilled(ImVec2(center.x+direction.x*9,center.y+direction.y*9),
            ImVec2(center.x-direction.x*5+direction.y*5,center.y-direction.y*5-direction.x*5),
            ImVec2(center.x-direction.x*5-direction.y*5,center.y-direction.y*5+direction.x*5),IM_COL32_WHITE);
    const auto north=radar({radarOrigin.x,radarOrigin.y+range,radarOrigin.z});
        const ImVec2 extent(vcsHud.x(12),12*vcsHud.scaleY);
        sprite("radar_north",ImVec2(north.x-extent.x*.5f,north.y-extent.y*.5f),extent,
            ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,209));
    if(!pcRadarFrame)draw->AddEllipse(center,ImVec2(radarRadius.x+1,radarRadius.y+1),accent,0,96,2);
    }
    }
    if(runtime.radioPopupTime>0&&!runtime.radioStationLabel.empty()) {
        const float opacity=std::clamp(std::min(runtime.radioPopupTime/0.5f,(3.0f-runtime.radioPopupTime)/0.15f),0.0f,1.0f);
        const auto color=IM_COL32(255,255,255,int(opacity*255));
        auto key=storylandRadioLogoKey(runtime.radioStationStem);
        if(!vcs&&key=="flash"&&retailHud&&retailHud->sprites.count("radio:flashback"))key="flashback";
        const auto spriteName=vcs?storylandVcsRadioSpriteName(runtime.radioStationStem):
            key.empty()?std::string{}:"radio:"+key;
        ImVec2 dimensions(128,96);
        if(retailHud) {
            const auto found=retailHud->sprites.find(spriteName);
            if(found!=retailHud->sprites.end()&&found->second&&found->second->height>0)
                dimensions.x=dimensions.y*float(found->second->width)/float(found->second->height);
        }
        const bool drawn=!spriteName.empty()&&sprite(spriteName,ImVec2((size.x-dimensions.x)*0.5f,28),
            dimensions,ImVec2(0,0),ImVec2(1,1),color);
        if(!vcs&&!drawn) {
            const auto extent=ImGui::CalcTextSize(runtime.radioStationLabel.c_str());
            text(ImVec2((size.x-extent.x)*0.5f,42),color,runtime.radioStationLabel.c_str());
        }
    }
    if (vcs && mission) {
        drawScriptFrameText(draw, retailHud, *mission, size, true);
    }
}


namespace {
uint32_t readRetailUtf8(const unsigned char*& cursor) {
    const uint32_t first=*cursor++;
    if(first<0x80)return first;
    const unsigned count=first>=0xC2&&first<=0xDF?1:first>=0xE0&&first<=0xEF?2:
        first>=0xF0&&first<=0xF4?3:0;
    if(!count)return 0xFFFD;
    uint32_t code=first&((1u<<(6-count))-1);
    const auto* next=cursor;
    for(unsigned index=0;index<count;++index) {
        if((*next&0xC0)!=0x80)return 0xFFFD;
        code=(code<<6)|(*next++&63);
    }
    cursor=next;
    if(code<(count==1?0x80u:count==2?0x800u:0x10000u)||code>0x10FFFF||
        (code>=0xD800&&code<=0xDFFF))return 0xFFFD;
    return code;
}

ImVec2 layoutRetailText(ImDrawList* draw,const StorylandRetailHudAssets* assets,ImVec2 position,
    ImU32 colour,const char* text,float height,float wrapWidth,const char* fontName,ImTextureID texture,bool centered=false,
    const StorylandTextLayout* layout=nullptr) {
    if(!text||!*text||!std::isfinite(height)||height<=0)return {};
    const StorylandTextLayout standard;
    const auto& format=layout?*layout:standard;
    if(!std::isfinite(format.horizontalScale)||format.horizontalScale<=0||
        !std::isfinite(format.lineSpacing)||format.lineSpacing<=0||
        !std::isfinite(format.rasterScaleX)||format.rasterScaleX<=0||
        !std::isfinite(format.rasterScaleY)||format.rasterScaleY<=0||
        !std::isfinite(format.cursorAdvanceQuantum)||format.cursorAdvanceQuantum<0)return {};
    const StorylandRetailFont* font=nullptr;
    const RgbaImage* raster=nullptr;
    if(assets) {
        const auto found=assets->fonts.find(fontName);
        if(found!=assets->fonts.end())font=&found->second;
        const auto image=assets->sprites.find(fontName);
        if(image!=assets->sprites.end()&&image->second&&image->second->width&&image->second->height)
            raster=image->second.get();
    }
    if(!raster)font=nullptr;
    const float scale=font?height/std::max(1u,unsigned(font->height)):1;
    const float horizontalScale=scale*(font?font->horizontalScale:1.0f)*format.horizontalScale;
    struct Character {
        uint32_t code=0;
        const StorylandRetailFontGlyph* glyph=nullptr;
        std::string fallback;
        float advance=0;
        float x=0,y=0;
        size_t line=0;
        ImU32 colour=0;
        int pcKey=-1;
        bool special=false,positioned=false;
    };
    const std::string expanded=format.pcHelpBindings?
        storylandExpandPcHelpControlText(text,*format.pcHelpBindings):storylandExpandRetailControlText(text);
    std::vector<Character> characters;
    ImU32 currentColour=colour;
    const auto* cursor=reinterpret_cast<const unsigned char*>(expanded.c_str());
    while(*cursor) {
        bool special=false;
        uint32_t code=readRetailUtf8(cursor);
        if(code=='~') {
            const auto* end=reinterpret_cast<const unsigned char*>(std::strchr(reinterpret_cast<const char*>(cursor),'~'));
            if(end==cursor) {code='~';cursor=end+1;}
            else if(end&&end-cursor==5&&*cursor=='#') {
                uint32_t escaped=0;bool valid=true;
                for(const auto* digit=cursor+1;digit<end;++digit) {
                    const unsigned value=*digit>='0'&&*digit<='9'?*digit-'0':
                        *digit>='A'&&*digit<='F'?*digit-'A'+10:
                        *digit>='a'&&*digit<='f'?*digit-'a'+10:16;
                    if(value>15) {valid=false;break;}
                    escaped=(escaped<<4)|value;
                }
                if(valid&&escaped&&!(escaped>=0xD800&&escaped<=0xDFFF)) {code=escaped;cursor=end+1;}
            }
            else if(end&&end==cursor+1) {
                const char token=char(*cursor);
                if(token=='n'||token=='N') {code='\n';cursor=end+1;}
                else if(token=='T'||token=='S'||token=='X'||token=='O') {
                    code=uint32_t(token);special=true;cursor=end+1;
                } else {
                    ImU32 replacement=0;
                    bool colourToken=true;
                    const int alpha=int((colour>>IM_COL32_A_SHIFT)&255);
                    switch(token) {
                    case 'R': case 'r':replacement=IM_COL32(255,80,80,alpha);break;
                    case 'G': case 'g':replacement=IM_COL32(80,220,100,alpha);break;
                    case 'B': case 'b':replacement=IM_COL32(100,150,255,alpha);break;
                    case 'W': case 'w':replacement=IM_COL32(255,255,255,alpha);break;
                    case 'Y': case 'y':replacement=IM_COL32(255,220,80,alpha);break;
                    case 'P': case 'p':replacement=IM_COL32(225,120,225,alpha);break;
                    case 'L': case 'l':replacement=IM_COL32(0,0,0,alpha);break;
                    case 's':replacement=colour;break;
                    case 'H': case 'h':replacement=IM_COL32(255,255,255,alpha);break;
                    default:colourToken=false;break;
                    }
                    if(colourToken) {
                        if(colour&~IM_COL32_A_MASK)currentColour=replacement;
                        cursor=end+1;continue;
                    }
                }
            }
        }
        if(code=='\r')continue;
        Character character;
        character.code=code;character.colour=currentColour;character.special=special;
        if(format.pcHelpBindings&&code>=StorylandPcHelpKeyBase&&
            code<=StorylandPcHelpKeyBase+StorylandPcHelpMouseMotion) {
            character.pcKey=int(code-StorylandPcHelpKeyBase);
            character.fallback=storylandPcHelpKeyLabel(character.pcKey);
            const float labelWidth=storylandMeasurePlayTestText(assets,character.fallback.c_str(),height*.72f,0,fontName).x;
            const char* sprite=storylandPcHelpButtonSprite(character.pcKey);
            const bool hasSprite=sprite&&assets&&assets->sprites.count(sprite)&&assets->sprites.at(sprite)&&
                assets->sprites.at(sprite)->width&&assets->sprites.at(sprite)->height;
            character.advance=(hasSprite?height:
                std::max(height,labelWidth+height*.5f))+height*.2f;
            characters.push_back(std::move(character));
            continue;
        }
        if(code==0xFFFD)character.fallback="?";
        else if(code<0x80)character.fallback.push_back(char(code));
        else {
            if(code<0x800)character.fallback.push_back(char(0xC0|(code>>6)));
            else {
                if(code<0x10000)character.fallback.push_back(char(0xE0|(code>>12)));
                else {
                    character.fallback.push_back(char(0xF0|(code>>18)));
                    character.fallback.push_back(char(0x80|((code>>12)&63)));
                }
                character.fallback.push_back(char(0x80|((code>>6)&63)));
            }
            character.fallback.push_back(char(0x80|(code&63)));
        }
        if(special)character.fallback=std::string("[")+char(code)+"]";
        if(code=='\n') {characters.push_back(std::move(character));continue;}
        if(font) {
            if(code<=0xFFFF)character.glyph=storylandRetailTextGlyph(*font,uint16_t(code),special);
            if(!character.glyph&&!special) {
                const uint32_t alias=code==0x2018||code==0x2019?'\'':
                    code==0x201C||code==0x201D?'"':code==0x2013||code==0x2014?'-':code==0xA0?' ':code;
                if(alias<=0xFFFF)character.glyph=storylandRetailTextGlyph(*font,uint16_t(alias));
            }
            if(!character.glyph&&!special&&code<0x80)character.glyph=storylandRetailTextGlyph(*font,'?');
        }
        character.advance=character.glyph?character.glyph->advance*horizontalScale:
            ImGui::GetFont()->CalcTextSizeA(height,FLT_MAX,0,character.fallback.c_str()).x*format.horizontalScale;
        if(format.fixedAdvance>0&&!special)character.advance=format.fixedAdvance;
        if(code=='\t')character.advance=height*2;
        characters.push_back(std::move(character));
    }
    float x=0,y=0;
    std::vector<float> lineWidths(1,0);
    std::vector<bool> wrappedLines(1,false);
    const auto newLine=[&](bool wrapped=false) {
        wrappedLines.back()=wrapped;x=0;y+=height*format.lineSpacing;
        lineWidths.push_back(0);wrappedLines.push_back(false);
    };
    for(size_t index=0;index<characters.size();++index) {
        auto& character=characters[index];
        if(character.code=='\n') {newLine();continue;}
        const bool space=character.code==' '||character.code=='\t';
        if(wrapWidth>0&&!space&&(index==0||characters[index-1].code==' '||
            characters[index-1].code=='\t'||characters[index-1].code=='\n')) {
            float wordWidth=0;
            for(size_t end=index;end<characters.size();++end) {
                const auto& next=characters[end];
                if(next.code==' '||next.code=='\t'||next.code=='\n')break;
                wordWidth+=next.advance;
            }
            if(x>0&&x+wordWidth>wrapWidth)newLine(true);
        }
        if(format.breakLongWords&&wrapWidth>0&&x>0&&x+character.advance>wrapWidth) {
            if(space)continue;
            newLine(true);
        }
        character.x=x;character.y=y;character.line=lineWidths.size()-1;character.positioned=true;
        x+=character.advance;
        if(!space)lineWidths.back()=x;
    }
    if(format.cursorAdvanceQuantum>0) {
        std::vector<float> cursors(lineWidths.size(),0);
        for(auto& character:characters) {
            if(!character.positioned)continue;
            auto& cursor=cursors[character.line];
            character.x=cursor;
            cursor+=std::floor(character.advance/format.cursorAdvanceQuantum)*format.cursorAdvanceQuantum;
        }
    }
    if(format.justify&&!centered&&!format.rightAligned&&wrapWidth>0) {
        std::vector<unsigned> spaces(lineWidths.size(),0);
        for(const auto& character:characters)if(character.code==' '&&
            character.x>0&&character.x<lineWidths[character.line])++spaces[character.line];
        std::vector<float> offsets(lineWidths.size(),0);
        for(auto& character:characters) {
            const auto line=character.line;
            if(!wrappedLines[line]||!spaces[line]||wrapWidth<=lineWidths[line])continue;
            const bool space=character.code==' '&&character.x>0&&character.x<lineWidths[line];
            character.x+=offsets[line];
            if(space)offsets[line]+=(wrapWidth-lineWidths[line])/float(spaces[line]);
        }
        for(size_t line=0;line<lineWidths.size();++line)if(wrappedLines[line]&&spaces[line])lineWidths[line]=wrapWidth;
    }
    for(const auto& character:characters) {
        const bool space=character.code==' '||character.code=='\t'||character.code=='\n';
        const float x=character.x-(centered?lineWidths[character.line]*0.5f:
            format.rightAligned?lineWidths[character.line]:0);
        const float y=character.y;
        if(draw&&!space) {
            if(character.pcKey>=0) {
                const float keyWidth=character.advance-height*.2f;
                const ImVec2 first(position.x+x,position.y+y);
                const ImVec2 last(first.x+keyWidth,first.y+height);
                const char* sprite=storylandPcHelpButtonSprite(character.pcKey);
                const ImTextureID keyTexture=sprite?storylandRetailSpriteTexture(assets,sprite):0;
                const int alpha=int((character.colour>>IM_COL32_A_SHIFT)&255);
                if(keyTexture)draw->AddImage(keyTexture,first,last,ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,alpha));
                else {
                    draw->AddRectFilled(first,last,IM_COL32(36,40,46,alpha),height*.12f);
                    draw->AddRect(first,last,IM_COL32(210,216,224,alpha),height*.12f);
                    float labelHeight=height*.72f;
                    float labelWidth=storylandMeasurePlayTestText(assets,character.fallback.c_str(),labelHeight,0,fontName).x;
                    if(labelWidth>keyWidth-height*.3f) {
                        const float scale=std::max(0.1f,(keyWidth-height*.3f)/labelWidth);
                        labelHeight*=scale;labelWidth*=scale;
                    }
                    storylandDrawPlayTestText(draw,assets,
                        ImVec2(first.x+(keyWidth-labelWidth)*.5f,first.y+(height-labelHeight)*.5f),
                        IM_COL32(255,255,255,alpha),character.fallback.c_str(),labelHeight,0,fontName);
                }
                continue;
            }
            const auto* glyph=character.glyph;
            bool atlasGlyph=false;
            if(glyph&&texture&&raster) {
                const float rasterWidth=glyph->atlasWidth?float(glyph->atlasWidth):
                    float(glyph->rasterWidth())/(character.special?1.33f:1.0f);
                const float rasterHeight=glyph->atlasHeight?float(glyph->atlasHeight):float(glyph->height);
                if(glyph->u+rasterWidth<=raster->width&&glyph->v+rasterHeight<=raster->height) {
                    const ImVec2 first(position.x+x-glyph->bearing*horizontalScale,position.y+y);
                    const ImVec2 last(first.x+glyph->rasterWidth()*horizontalScale*format.rasterScaleX,
                        first.y+glyph->height*scale*format.rasterScaleY);
                    const float inset=format.insetTextureCoordinates?.5f:0;
                    draw->AddImage(texture,first,last,
                        ImVec2((glyph->u+inset)/raster->width,float(glyph->v)/raster->height),
                        ImVec2((glyph->u+rasterWidth-inset)/raster->width,
                            (glyph->v+rasterHeight-inset)/raster->height),character.colour);
                    atlasGlyph=true;
                }
            }
            if(!atlasGlyph) {
                const int first=draw->VtxBuffer.Size;
                draw->AddText(ImGui::GetFont(),height*format.rasterScaleY,ImVec2(position.x+x,position.y+y),
                    character.colour,character.fallback.c_str());
                const float fallbackScale=format.horizontalScale*format.rasterScaleX/format.rasterScaleY;
                if(fallbackScale!=1.0f)for(int index=first;index<draw->VtxBuffer.Size;++index)
                    draw->VtxBuffer[index].pos.x=position.x+x+
                        (draw->VtxBuffer[index].pos.x-position.x-x)*fallbackScale;
            }
        }
    }
    return ImVec2(*std::max_element(lineWidths.begin(),lineWidths.end()),y+height);
}

}

std::string storylandExpandRetailControlText(const std::string& text) {
    const std::pair<const char*, const char*> bindings[] = {
        {"PED_SPRINT", "Shift"}, {"PDSPR", "Shift"},
        {"PED_JUMPING", "Space"}, {"SWIMO", "Space"},
        {"VEHICLE_ENTER_EXIT", "F"}, {"VEEE", "F"},
        {"PED_FIREWEAPON", "G / Mouse 1"}, {"PDFW", "G / Mouse 1"},
        {"VEHICLE_HANDBRAKE", "Space"},
        {"VEHICLE_ACCELERATE", "W / Up"}, {"VEACC", "W / Up"},
        {"VEHICLE_BRAKE", "S / Down"}, {"VEBRK", "S / Down"}, {"VEHB", "S / Down"},
        {"PDLT", "E"}, {"FREE1", "Mouse 2"}, {"FREE2", "Mouse"},
        {"SWIMD", "W / A / S / D"}, {"SWIMF", "Shift"}, {"VEHN", "H"},
        {"VEWEP", "G / Mouse 1"}, {"FIREH", "I / J / K / L"}
    };
    std::string expanded = text;
    size_t control = 0;
    while ((control = expanded.find("~k~", control)) != std::string::npos) {
        size_t opening = control + 3u;
        while (opening < expanded.size() &&
            std::isspace(static_cast<unsigned char>(expanded[opening]))) ++opening;
        if (opening >= expanded.size() || expanded[opening] != '~') {
            control += 3u;
            continue;
        }
        const size_t end = expanded.find('~', opening + 1u);
        if (end == std::string::npos) break;
        const std::string action = expanded.substr(opening + 1u, end - opening - 1u);
        if (action.empty()) {
            control = end + 1u;
            continue;
        }
        std::string label = action;
        for (const auto& binding : bindings) {
            if (action == binding.first) {
                label = binding.second;
                break;
            }
        }
        std::replace(label.begin(), label.end(), '_', ' ');
        label = "[" + label + "]";
        expanded.replace(control, end - control + 1u, label);
        control += label.size();
    }
    return expanded;
}

ImVec2 storylandMeasurePlayTestText(const StorylandRetailHudAssets* assets,const char* text,
    float height,float wrapWidth,const char* fontName,const StorylandTextLayout* layout) {
    return layoutRetailText(nullptr,assets,{},0,text,height,wrapWidth,fontName,0,false,layout);
}
void storylandDrawPlayTestText(ImDrawList* draw,const StorylandRetailHudAssets* assets,ImVec2 position,
    ImU32 colour,const char* text,float height,float wrapWidth,const char* fontName,bool centered,
    const StorylandTextLayout* layout) {
    const ImTextureID texture=assets&&assets->fonts.count(fontName)?
        storylandRetailSpriteTexture(assets,fontName):0;
    layoutRetailText(draw,assets,position,colour,text,height,wrapWidth,fontName,texture,centered,layout);
}

void storylandDrawLcsScriptTexts(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& world,ImVec2 viewport,
    const std::function<std::string(const std::string&,const std::string&)>& translate) {
    if(!draw||viewport.x<=0||viewport.y<=0)return;
    // The native PS2 script-text branch uses direct coordinates on its 640x448 canvas.
    const float sx=viewport.x/640.0f,sy=viewport.y/448.0f;
    draw->PushClipRect(ImVec2(0,0),viewport,true);
    for(const auto& line:world.lcsScriptTexts) {
        if(line.key.empty()||line.scaleX<=0||line.scaleY<=0)continue;
        std::string text=translate?translate(line.key,line.missionText):line.key;
        size_t nextNumber=0;
        for(const auto number:line.numbers) {
            const auto token=text.find("~1~",nextNumber);
            if(token==std::string::npos)break;
            const auto replacement=std::to_string(number);
            text.replace(token,3,replacement);nextNumber=token+replacement.size();
        }
        // Native intro lines have 100 UTF-16 cells, including the terminator.
        const auto* begin=reinterpret_cast<const unsigned char*>(text.c_str());
        const auto* cursor=begin;size_t cells=0;
        while(*cursor&&cells<99) {
            const auto* previous=cursor;const auto code=readRetailUtf8(cursor);
            cells+=code>0xFFFF?2:1;
            if(cells>99) {cursor=previous;break;}
        }
        text.resize(size_t(cursor-begin));
        const ImVec2 at(line.x*sx,line.y*sy);
        const float height=20.0f*line.scaleY*sy;
        if(!std::isfinite(height)||height<=0||height>65536||!std::isfinite(at.x)||!std::isfinite(at.y))continue;
        StorylandTextLayout format;
        format.horizontalScale=2.0f*line.scaleX*sx/(line.scaleY*sy);
        format.lineSpacing=0.9f;format.fixedAdvance=line.proportional?0:30.0f*line.scaleX*sx;
        format.rightAligned=line.rightJustify;format.justify=line.justify;
        const float width=line.centered?640.0f*sx:line.rightJustify?at.x:(line.wrapX-line.x)*sx;
        if(!std::isfinite(width)||!std::isfinite(format.horizontalScale)||
            !std::isfinite(format.fixedAdvance)||!std::isfinite(line.wrapX*sx))continue;
        const float wrap=std::max(1.0f,width);
        const auto extent=storylandMeasurePlayTestText(assets,text.c_str(),height,wrap,"gametext",&format);
        if(line.background) {
            const float left=line.centered?at.x-320.0f*sx-4.0f*sx:at.x-4.0f*sx;
            const float right=line.centered?at.x+320.0f*sx+4.0f*sx:line.wrapX*sx;
            if(right>left)draw->AddRectFilled(ImVec2(left,at.y-2.0f*sy),
                ImVec2(right,at.y+extent.y+2.0f*sy),IM_COL32(128,128,128,128));
        }
        storylandDrawPlayTestText(draw,assets,at,IM_COL32(line.color[0],line.color[1],line.color[2],line.color[3]),
            text.c_str(),height,wrap,"gametext",line.centered,&format);
    }
    draw->PopClipRect();
}

std::string storylandFormatLcsMessage(const StorylandMissionEnvironment::LcsMessage& message,
    const std::function<std::string(const std::string&,const std::string&)>& translate) {
    const auto lookup=[&](const std::string& key) {return translate?translate(key,message.missionText):key;};
    std::string text=lookup(message.key);
    size_t cursor=0,index=0;
    while((cursor=text.find("~1~",cursor))!=std::string::npos) {
        const size_t slot=std::min(size_t(5),index++);
        const auto value=std::to_string(slot<message.numbers.size()?message.numbers[slot]:-1);
        text.replace(cursor,3,value);cursor+=value.size();
    }
    if(!message.stringKey.empty()) {
        const auto inserted=lookup(message.stringKey);cursor=0;
        while((cursor=text.find("~a~",cursor))!=std::string::npos) {
            text.replace(cursor,3,inserted);cursor+=inserted.size();
        }
    }
    const auto* begin=reinterpret_cast<const unsigned char*>(text.c_str());
    const auto* end=begin;size_t cells=0;
    while(*end&&cells<255) {
        const auto* previous=end;const auto code=readRetailUtf8(end);
        cells+=code>0xFFFF?2:1;
        if(cells>255) {end=previous;break;}
    }
    text.resize(size_t(end-begin));return text;
}

void storylandDrawLcsMessages(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& world,ImVec2 viewport,bool afterFade,
    const std::function<std::string(const std::string&,const std::string&)>& translate) {
    if(!draw||!world.lcsMessageQueuesUsed||viewport.x<=0||viewport.y<=0)return;
    const float sx=viewport.x/640.0f,sy=viewport.y/448.0f;
    draw->PushClipRect(ImVec2(0,0),viewport,true);
    const auto outlined=[&](const std::string& text,ImVec2 at,float height,float width,const char* font,
        ImU32 color,bool centered,bool right) {
        StorylandTextLayout format;format.rightAligned=right;
        storylandDrawPlayTestText(draw,assets,ImVec2(at.x+2*sx,at.y+2*sy),IM_COL32(0,0,0,220),
            text.c_str(),height,width,font,centered,&format);
        storylandDrawPlayTestText(draw,assets,at,color,text.c_str(),height,width,font,centered,&format);
    };
    for(size_t style=0;style<world.lcsBigMessages.size();++style) {
        const auto& lane=world.lcsBigMessages[style];
        if(lane.empty()||afterFade==(style==0||style==2))continue;
        // Native mission titles suppress the overlapping odd-job overlays.
        if((style==4||style==5)&&!world.lcsBigMessages[1].empty())continue;
        const auto& message=lane.front();const auto text=storylandFormatLcsMessage(message,translate);
        if(style==1) {
            if(!assets||!assets->fonts.count("fancytext")||!assets->sprites.count("fonts:font2"))continue;
            const ImVec2 scale(viewport.x/480.0f,viewport.y/272.0f);
            const float horizontal=viewport.x/viewport.y>1.5f?
                0.42768001556396484f:0.47520002722740173f;
            const float height=40.0f*.88f*scale.y;
            const float lineHeight=18.0f*.88f*scale.y;
            const float width=456.0f*scale.x;
            StorylandTextLayout format;
            format.horizontalScale=horizontal*scale.x/(.88f*scale.y);
            format.lineSpacing=18.0f/40.0f;
            format.rightAligned=true;
            format.rasterScaleX=.96f;
            format.rasterScaleY=.5f;
            format.insetTextureCoordinates=false;
            format.cursorAdvanceQuantum=.75f*scale.x;
            format.breakLongWords=false;
            const auto measured=storylandMeasurePlayTestText(assets,text.c_str(),height,
                480.0f*scale.x,"fancytext",&format);
            const float lines=std::max(1.0f,std::round((measured.y-height)/lineHeight)+1.0f);
            const ImVec2 at(456.0f*scale.x,256.0f*scale.y-lines*lineHeight);
            storylandDrawPlayTestText(draw,assets,ImVec2(at.x+1.5f*scale.x,at.y+(17.0f/14.0f)*scale.y),
                IM_COL32(0,0,0,255),text.c_str(),height,width,"fancytext",false,&format);
            storylandDrawPlayTestText(draw,assets,at,IM_COL32(220,172,2,255),
                text.c_str(),height,width,"fancytext",false,&format);
            continue;
        }
        ImVec2 at(viewport.x*.5f,viewport.y*.5f-18*sy);
        float height=36*sy,width=600*sx;const char* font="bigtext";
        bool centered=true,right=false;
        ImU32 color=IM_COL32(85,119,133,255);
        if(style==2) {at={viewport.x-20*sx,viewport.y-122*sy};height=40*sy;centered=false;right=true;color=IM_COL32(170,123,87,255);}
        else if(style==3||style==4) {at.y=viewport.y*.5f-80*sy;height=30*sy;font="fancytext";color=IM_COL32(89,115,150,255);}
        else if(style==5) {at.y=viewport.y*.5f-20*sy;height=24*sy;font="fancytext";color=IM_COL32(156,91,40,255);}
        else if(style>=6) {
            // LCS added two lanes beyond the six styles drawn by the reference HUD.
            // They keep independent timing and use the readable standard font.
            at.y=viewport.y*(style==6?.32f:.62f);height=26*sy;font="gametext";color=IM_COL32_WHITE;
        }
        outlined(text,at,height,width,font,color,centered,right);
    }
    if(afterFade&&!world.lcsBriefMessages.empty()&&world.lcsBriefMessages.front().overrideFade&&
        world.lcsBigMessages[2].empty()) {
        const auto text=storylandFormatLcsMessage(world.lcsBriefMessages.front(),translate);
        const float width=std::min(680.0f,viewport.x-80.0f),height=std::clamp(viewport.y/28.0f,18.0f,30.0f);
        const auto extent=storylandMeasurePlayTestText(assets,text.c_str(),height,width);
        outlined(text,ImVec2(viewport.x*.5f,viewport.y-std::max(28.0f,viewport.y*.06f)-extent.y),height,width,
            "gametext",IM_COL32_WHITE,true,false);
    }
    draw->PopClipRect();
}

namespace {
int32_t scriptCoordinateAdd(int32_t first,int32_t second) {
    const uint32_t bits=uint32_t(first)+uint32_t(second);
    int32_t result;std::memcpy(&result,&bits,sizeof(result));return result;
}
int32_t scriptCoordinateSubtract(int32_t first,int32_t second) {
    const uint32_t bits=uint32_t(first)-uint32_t(second);
    int32_t result;std::memcpy(&result,&bits,sizeof(result));return result;
}
int32_t scriptTruncate(float value) {
    if(!std::isfinite(value))return 0;
    return int32_t(std::clamp(double(value),double(INT32_MIN),double(INT32_MAX)));
}
int32_t scriptMetric(float value) {
    if(!std::isfinite(value))return 0;
    return int32_t(std::clamp(std::ceil(double(value)),double(INT32_MIN),double(INT32_MAX)));
}
ImU32 scriptColor(const std::array<uint8_t,4>& color) {
    return IM_COL32(color[0],color[1],color[2],color[3]);
}
std::string scriptTextUtf8(const std::u16string& value) {
    std::string result;
    for(size_t index=0;index<value.size();++index) {
        uint32_t code=value[index];
        if(code>=0xD800&&code<=0xDBFF&&index+1<value.size()&&value[index+1]>=0xDC00&&value[index+1]<=0xDFFF)
            code=0x10000+((code-0xD800)<<10)+(value[++index]-0xDC00);
        else if(code>=0xD800&&code<=0xDFFF)code=0xFFFD;
        if(code<0x80)result.push_back(char(code));
        else {
            if(code<0x800)result.push_back(char(0xC0|(code>>6)));
            else {
                if(code<0x10000)result.push_back(char(0xE0|(code>>12)));
                else {result.push_back(char(0xF0|(code>>18)));result.push_back(char(0x80|((code>>12)&63)));}
                result.push_back(char(0x80|((code>>6)&63)));
            }
            result.push_back(char(0x80|(code&63)));
        }
    }
    return result;
}
ImTextureID scriptHudTexture(const StorylandRetailHudAssets* assets,const std::string& name) {
#ifdef STORYLAND_NATIVE_HUD_OPENGL
    static StorylandHudContext context=nullptr;
    static const StorylandRetailHudAssets* previous=nullptr;
    static uint64_t generation=UINT64_MAX;
    static std::map<const RgbaImage*,GLuint> textures;
    const auto current=storylandCurrentHudContext();
    if(current!=context||previous!=assets||(assets&&generation!=assets->generation)) {
        if(context==current)for(const auto& entry:textures)glDeleteTextures(1,&entry.second);
        textures.clear();context=current;previous=assets;generation=assets?assets->generation:0;
    }
    if(!current||!assets)return 0;
    const auto found=assets->sprites.find(lowerHudName(name));
    if(found==assets->sprites.end()||!found->second)return 0;
    const auto* image=found->second.get();
    auto texture=textures.find(image);
    if(texture==textures.end()) {
        GLint binding=0,alignment=4;glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
        GLuint handle=0;glGenTextures(1,&handle);if(!handle)return 0;
        glBindTexture(GL_TEXTURE_2D,handle);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        storylandUploadRgbaTexture(*image);
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glBindTexture(GL_TEXTURE_2D,GLuint(binding));
        texture=textures.emplace(image,handle).first;
    }
    return static_cast<ImTextureID>(texture->second);
#else
    (void)assets;(void)name;return 0;
#endif
}
void drawScriptTextLayout(ImDrawList* draw,const StorylandRetailHudAssets* assets,const char* fontName,
    const StorylandScriptTextLayout& layout,ImVec2 scale,ImVec2 offset={}) {
    if(!draw)return;
    const RgbaImage* raster=nullptr;
    ImTextureID texture=0;
    if(assets) {
        const auto image=assets->sprites.find(fontName);
        if(image!=assets->sprites.end()&&image->second) {
            raster=image->second.get();
            texture=scriptHudTexture(assets,fontName);
        }
    }
    for(const auto& item:layout.glyphs) {
        if(item.unicodeFallback) {
            if(!ImGui::GetCurrentContext()||!ImGui::GetFont()||item.size.y<=0)continue;
            const unsigned int code=item.code;
            char utf8[4]={};
            if(code<0x800) {utf8[0]=char(0xC0|(code>>6));utf8[1]=char(0x80|(code&63));}
            else {utf8[0]=char(0xE0|(code>>12));utf8[1]=char(0x80|((code>>6)&63));utf8[2]=char(0x80|(code&63));}
            const ImVec2 first((item.position.x+offset.x)*scale.x,(item.position.y+offset.y)*scale.y);
            ImFont* fallbackFont=ImGui::GetFont();
            const float fallbackHeight=item.size.y*scale.y;
            const float fallbackWidth=fallbackFont->CalcTextSizeA(fallbackHeight,FLT_MAX,0,utf8).x;
            const int firstVertex=draw->VtxBuffer.Size;
            draw->AddText(fallbackFont,fallbackHeight,first,item.color,utf8);
            if(fallbackWidth>0) {
                const float horizontalScale=item.size.x*scale.x/fallbackWidth;
                for(int index=firstVertex;index<draw->VtxBuffer.Size;++index)
                    draw->VtxBuffer[index].pos.x=first.x+(draw->VtxBuffer[index].pos.x-first.x)*horizontalScale;
            }
            continue;
        }
        if(!texture||!raster)continue;
        const auto& glyph=item.glyph;
        const float width=glyph.atlasWidth?float(glyph.atlasWidth):
            float(glyph.rasterWidth())/(item.special?1.33f:1.0f);
        const float height=glyph.atlasHeight?float(glyph.atlasHeight):float(glyph.height);
        if(width<=0||height<=0||glyph.u+width>raster->width||glyph.v+height>raster->height)continue;
        const ImVec2 first((item.position.x+offset.x)*scale.x,(item.position.y+offset.y)*scale.y);
        draw->AddImage(texture,first,ImVec2(first.x+item.size.x*scale.x,first.y+item.size.y*scale.y),
            ImVec2((glyph.u+.5f)/raster->width,float(glyph.v)/raster->height),
            ImVec2((glyph.u+width-.5f)/raster->width,(glyph.v+height-.5f)/raster->height),item.color);
    }
}
}

StorylandScriptTextLayout storylandLayoutScriptText(const StorylandRetailFont& font,
    const std::u16string& text,float scale,bool widescreen,bool proportional,int32_t x,int32_t y,
    const ImVec4& clip,int32_t alignment,ImU32 color,bool correctRightEdge,bool wordWrap,
    bool preciseGlyphGeometry) {
    StorylandScriptTextLayout result;
    if(text.empty()||!std::isfinite(scale)||scale<0)return result;
    const float wideFactor=widescreen?.75f:1.0f;
    const float horizontal=scale*wideFactor;
    result.lineHeight=scriptMetric(font.height*scale);
    struct Character {
        uint16_t code=0;const StorylandRetailFontGlyph* glyph=nullptr;
        int32_t advance=0;ImU32 color=0;bool special=false,unicodeFallback=false;
    };
    std::vector<Character> characters;
    ImU32 current=color;
    for(size_t index=0;index<text.size();++index) {
        uint16_t code=text[index];bool special=false;
        if(code=='\r')continue;
        if(code=='~'&&index+2<text.size()&&text[index+2]=='~') {
            const uint16_t token=text[index+1];
            const int alpha=int((color>>IM_COL32_A_SHIFT)&255);
            ImU32 replacement=0;bool colorToken=true;
            switch(token) {
            case 'R': case 'r':replacement=IM_COL32(255,0,0,alpha);break;
            case 'G': case 'g':replacement=IM_COL32(0,255,0,alpha);break;
            case 'B': case 'b':replacement=IM_COL32(0,0,255,alpha);break;
            case 'W': case 'w':replacement=IM_COL32(255,255,255,alpha);break;
            case 'Y': case 'y':replacement=IM_COL32(255,255,0,alpha);break;
            case 'P': case 'p':replacement=IM_COL32(255,0,255,alpha);break;
            case 'L': case 'l':replacement=IM_COL32(0,0,0,alpha);break;
            case 's':replacement=color;break;
            case 'H': case 'h':replacement=IM_COL32(255,255,255,alpha);break;
            default:colorToken=false;break;
            }
            if(colorToken) {current=replacement;index+=2;continue;}
            if(token=='n'||token=='N') {code='\n';index+=2;}
            else if(font.specialGlyphs.count(token)) {code=token;special=true;index+=2;}
        }
        Character character;character.code=code;character.color=current;character.special=special;
        character.glyph=storylandRetailTextGlyph(font,code,special);
        if(character.glyph) {
            const float advance=float(proportional?character.glyph->advance:font.width);
            character.advance=scriptMetric(preciseGlyphGeometry?(advance*scale)*wideFactor:advance*horizontal);
        }
        else if(!special&&code>=0xA0&&(code<0xD800||code>0xDFFF)) {
            character.unicodeFallback=true;
            character.advance=scriptMetric(preciseGlyphGeometry?
                (float(font.width)*scale)*wideFactor:float(font.width)*horizontal);
        }
        characters.push_back(character);
    }
    struct Line {size_t first=0,last=0;int32_t width=0;};
    const auto makeLines=[&](int32_t start,float right) {
        std::vector<Line> lines;
        size_t first=0,index=0;int32_t width=0,trimmed=0;
        while(index<characters.size()) {
            const auto& character=characters[index];
            if(character.code=='\n') {
                lines.push_back({first,index,trimmed});++index;first=index;width=trimmed=0;continue;
            }
            if(wordWrap&&character.code!=' '&&(index==first||characters[index-1].code==' ')) {
                int64_t word=0;
                for(size_t end=index;end<characters.size()&&characters[end].code!=' '&&characters[end].code!='\n';++end)
                    word+=characters[end].advance;
                if(trimmed>0&&double(start)+width+word>right) {
                    lines.push_back({first,index,trimmed});first=index;width=trimmed=0;
                }
            }
            width=scriptCoordinateAdd(width,character.advance);
            if(character.code!=' ')trimmed=width;
            ++index;
        }
        if(first<characters.size()||lines.empty())lines.push_back({first,characters.size(),trimmed});
        return lines;
    };
    if(correctRightEdge) {
        const auto measured=makeLines(0,478);
        const int32_t width=measured.empty()?0:measured.front().width;
        if(scriptCoordinateAdd(x,width)>480)x=std::max(0,scriptCoordinateSubtract(480,width));
    }
    result.originX=x;result.originY=y;
    const bool alignLeft=(alignment&1)!=0,alignCenter=!alignLeft&&(alignment&2)!=0;
    const bool alignRight=!alignLeft&&!alignCenter&&(alignment&4)!=0;
    const int32_t start=(alignLeft||alignCenter||alignRight)?scriptTruncate(clip.x):x;
    const auto lines=makeLines(start,clip.z);
    int32_t top=y;
    if(alignment&8)top=scriptTruncate(clip.y);
    else if(alignment&16)top=scriptTruncate(clip.y+(clip.w-clip.y)*.5f-float(lines.size())*float(std::max(0,result.lineHeight))*.5f);
    else if(alignment&32)top=scriptTruncate(clip.w-float(lines.size())*float(std::max(0,result.lineHeight)));
    for(size_t row=0;row<lines.size();++row) {
        const auto& line=lines[row];result.lineWidths.push_back(line.width);
        int32_t left=start;
        if(alignCenter)left=scriptTruncate(clip.x+(clip.z-clip.x)*.5f-float(line.width/2));
        else if(alignRight)left=scriptTruncate(clip.z-float(line.width));
        int32_t pen=left;
        const int32_t baseline=scriptCoordinateAdd(top,int32_t(uint32_t(row)*uint32_t(result.lineHeight)));
        for(size_t index=line.first;index<line.last;++index) {
            const auto& character=characters[index];
            if(character.glyph&&character.code!=' ') {
                const auto& glyph=*character.glyph;
                StorylandScriptGlyph item;item.glyph=glyph;item.code=character.code;
                item.special=character.special;item.color=character.color;
                if(preciseGlyphGeometry) {
                    const float left=float(pen)-(glyph.bearing*scale)*wideFactor;
                    const float right=float(pen)+((glyph.advance+glyph.trailing)*scale)*wideFactor;
                    item.position={left,float(baseline)};
                    item.size={right-left,glyph.height*scale};
                } else {
                    item.position={float(scriptCoordinateSubtract(pen,scriptMetric(glyph.bearing*horizontal))),float(baseline)};
                    item.size={float(scriptMetric(glyph.rasterWidth()*horizontal)),float(scriptMetric(glyph.height*scale))};
                }
                result.glyphs.push_back(item);
            } else if(character.unicodeFallback) {
                StorylandScriptGlyph item;item.code=character.code;item.color=character.color;
                item.position={float(pen),float(baseline)};
                item.size={float(character.advance),float(result.lineHeight)};
                item.unicodeFallback=true;
                result.glyphs.push_back(item);
            }
            pen=scriptCoordinateAdd(pen,character.advance);
        }
    }
    return result;
}

std::vector<StorylandOpticQuad> storylandManualAimOverlay(int32_t weapon,int32_t mode,bool widescreen) {
    std::vector<StorylandOpticQuad> result;
    const auto quad=[&](const char* texture,float left,float top,float right,float bottom,
        std::array<ImVec2,4> uv) {
        result.push_back({texture,{{{left,top},{right,top},{right,bottom},{left,bottom}}},uv});
    };
    if(mode==8) {
        // SLUS 0x32358C: the rocket sight is a single native sprite centered
        // in the 640 x 448 sprite coordinate space, with half extents 40.
        quad("siterocket",210,136-40*272.0f/448.0f,270,136+40*272.0f/448.0f,
            {{{0,0},{1,0},{1,1},{0,1}}});
        return result;
    }
    if(mode!=7&&mode!=46&&mode!=47)return result;
    // 0x323648..0x32398C mirrors a complete quarter raster with deliberate
    // .01/.99 UV insets. It does not stretch one whole scope over the viewport.
    const char* texture=mode==46?"viewfinder_128":mode==47?"binox_128":weapon==31?"sitelaser":"sitesniper";
    const float halfWidth=(mode==46?238.0f:mode==47?241.0f:280.0f)*(widescreen?.75f:1.0f);
    const float halfHeight=mode==46||mode==47?102.0f:210.0f;
    quad(texture,240-halfWidth,136-halfHeight,240,136,{{{.01f,.01f},{1,0},{1,1},{.01f,1}}});
    quad(texture,240,136-halfHeight,240+halfWidth,136,{{{.99f,0},{.01f,.01f},{.01f,1},{.99f,1}}});
    quad(texture,240-halfWidth,136,240,136+halfHeight,{{{.01f,.99f},{1,.99f},{1,.01f},{.01f,.01f}}});
    quad(texture,240,136,240+halfWidth,136+halfHeight,{{{.99f,.99f},{.01f,.99f},{.01f,.01f},{.99f,.01f}}});
    return result;
}

ImU32 storylandTargetHealthColor(float health,float maximumHealth) {
    // Native319678 interpolates these three RGB knots at0,.5,1. A dead
    // target is black; the native CPed+828 divisor belongs to this reticle.
    if(!std::isfinite(health)||!std::isfinite(maximumHealth)||health<=0||maximumHealth<=0)
        return IM_COL32(0,0,0,255);
    const float ratio=std::clamp(health/maximumHealth,0.0f,1.0f);
    const float t=ratio<=.5f?ratio*2:(ratio-.5f)*2;
    const int red=int(ratio<=.5f?222.0f*(1-t)+255.0f*t:255.0f*(1-t));
    const int green=int(ratio<=.5f?165.0f*t:165.0f*(1-t)+222.0f*t);
    return IM_COL32(red,green,0,255);
}

ImU32 storylandTargetIndicatorColor(bool vcs,bool character,float healthRatio) {
    // Both games mark a newly acquired object red (VCS2366B4/LCS34B92C).
    // VCS319678 maps character health through three RGB knots; LCS2B61C8
    // retains green hue and scales HSV value, with its original alpha225.
    ImU32 nativeColor=IM_COL32(255,0,0,255);
    if(character) {
        const float ratio=std::isfinite(healthRatio)?std::clamp(healthRatio,0.0f,1.0f):0;
        nativeColor=vcs?storylandTargetHealthColor(ratio,1):IM_COL32(0,int(ratio*255),0,225);
    }
    return IM_COL32(((nativeColor>>IM_COL32_R_SHIFT)&255)*158/256,
        ((nativeColor>>IM_COL32_G_SHIFT)&255)*158/256,
        ((nativeColor>>IM_COL32_B_SHIFT)&255)*158/256,
        (nativeColor>>IM_COL32_A_SHIFT)&255);
}

std::vector<StorylandOpticQuad> storylandTargetIndicatorQuads(ImVec2 center,float depth,float aspect) {
    std::vector<StorylandOpticQuad> quads;
    if(!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(depth)||depth<=0||
        !std::isfinite(aspect)||aspect<=0)return quads;
    // VCS319454..3195C8 and LCS2B6624..2B67C0 use particle:triangle16 in 640x448
    // sprite space; retain its depth-dependent spacing and half dimensions.
    const float width=640.0f/(depth*aspect),height=448.0f/depth;
    if(!std::isfinite(width)||!std::isfinite(height))return quads;
    const float halfX=10.0f-std::max(80.0f-height,0.0f)*.08f;
    const float halfY=(10.0f-std::max(80.0f-width,0.0f)*.08f)*.8f;
    const ImVec2 spriteCenter(center.x/.75f,center.y*(448.0f/272.0f));
    if(!std::isfinite(spriteCenter.x)||!std::isfinite(spriteCenter.y))return quads;
    const std::array<ImVec2,3> centers{{{spriteCenter.x-width*.4f,spriteCenter.y-height*.2f},
        {spriteCenter.x+width*.4f,spriteCenter.y-height*.2f},{spriteCenter.x,spriteCenter.y+height*.5f}}};
    constexpr float angles[]={-.8f,3.94159265359f,1.57079632679f};
    for(size_t index=0;index<centers.size();++index) {
        StorylandOpticQuad quad;quad.texture="triangle16";
        quad.uv={{{0,0},{1,0},{1,1},{0,1}}};
        const float c=std::cos(angles[index]),s=std::sin(angles[index]);
        const std::array<ImVec2,4> local{{{-halfX,-halfY},{halfX,-halfY},{halfX,halfY},{-halfX,halfY}}};
        // Native 3A1E68..3A1EE0 rotates sprite axes clockwise in screen space.
        for(size_t corner=0;corner<4;++corner) {
            quad.corners[corner]={(centers[index].x+local[corner].x*c+local[corner].y*s)*.75f,
                (centers[index].y-local[corner].x*s+local[corner].y*c)*(272.0f/448.0f)};
            if(!std::isfinite(quad.corners[corner].x)||!std::isfinite(quad.corners[corner].y))return {};
        }
        quads.push_back(std::move(quad));
    }
    return quads;
}

bool storylandResolveTargetIndicator(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,StorylandTargetIndicator& indicator) {
    indicator={};
    if(!storylandHasPlayTestTarget(runtime))return false;
    const auto finite=[](const StorylandNativePoint& point) {
        return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
    };
    const auto* mission=runtime.missionEnvironment;
    const auto character=[&](int32_t handle) {
        if(!mission)return false;
        const auto found=mission->missionCharacters.find(handle);
        if(found==mission->missionCharacters.end())return false;
        const auto& actor=found->second;
        if(!actor.alive||actor.health<=0||!actor.visible||actor.renderAlpha==0)return false;
        indicator.origin={actor.x,actor.y,actor.z};
        indicator.health=actor.health;indicator.maximumHealth=actor.maximumHealth;
        indicator.character=true;
        const auto pose=runtime.missionCombat.find(handle);
        if(pose!=runtime.missionCombat.end()&&pose->second.anchorsValid&&pose->second.nativeRootValid&&
            finite(pose->second.head)&&finite(pose->second.nativeRoot)) {
            const auto& root=pose->second.nativeRoot;
            indicator.position={root.x,root.y,(root.z+pose->second.head.z)*.5f};
            indicator.positionValid=true;
        }
        return finite(indicator.origin);
    };
    if(runtime.targetedPed>=0) {
        const size_t index=size_t(runtime.targetedPed);
        if(index>=runtime.pedestrians.size()||index>=pedestrians.size())return false;
        const auto& ped=runtime.pedestrians[index];
        if(ped.health<=0||ped.state==StorylandPedState::Dead)return false;
        if(ped.scriptHandle>=0&&character(ped.scriptHandle))return true;
        indicator.origin=pedestrians[index].position;
        indicator.health=ped.health;indicator.maximumHealth=100;
        indicator.character=true;
        if(ped.headValid&&ped.nativeRootValid&&finite(ped.head)&&finite(ped.nativeRoot)) {
            indicator.position={ped.nativeRoot.x,ped.nativeRoot.y,(ped.nativeRoot.z+ped.head.z)*.5f};
            indicator.positionValid=true;
        }
        return finite(indicator.origin);
    }
    if(!mission)return false;
    if(mission->playerTarget.kind==StorylandMissionEnvironment::TargetKind::Character)
        return character(mission->playerTarget.handle);
    if(mission->playerTarget.kind!=StorylandMissionEnvironment::TargetKind::Object)return false;
    const auto found=std::find_if(mission->objects.begin(),mission->objects.end(),[&](const auto& object) {
        return object.handle==mission->playerTarget.handle;
    });
    if(found==mission->objects.end()||!found->damage.visible)return false;
    indicator.origin={found->x,found->y,found->z};
    indicator.health=indicator.maximumHealth=1;
    if(mission->objectWorld) {
        const auto collision=mission->objectWorld->collisionModels.find(uint32_t(found->model));
        if(collision!=mission->objectWorld->collisionModels.end()) {
            const float radius=collision->second.boundingRadius;
            if(std::isfinite(radius)&&radius>0) {
                // VCS3471D8/LCS2BBA18 return [cx-r,cy+r,cx+r,cy-r]. The
                // reticle subtracts .33*(bottom-top), then halves that offset.
                indicator.position={found->x,found->y,found->z+radius*.33f};
                indicator.positionValid=finite(indicator.position);
            }
        }
    }
    return finite(indicator.origin);
}

static bool equippedWeaponIsFirearm(const StorylandPlayTestRuntime& runtime) {
    if(runtime.equippedWeapon>=runtime.weapons.size()||runtime.equippedWeapon>=runtime.inventory.size()||
        !runtime.inventory[runtime.equippedWeapon].owned)return false;
    const auto& weapon=runtime.weapons[runtime.equippedWeapon];
    return weapon.nativeId>=0&&(weapon.fireType==1||weapon.fireType==3||
        (weapon.fireType==2&&weapon.slot==7));
}

void storylandDrawTargetIndicator(ImDrawList* draw,ImTextureID texture,ImTextureID firearmSight,
    const StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandStoriesCamera& camera,ImVec2 displaySize,float nearClip) {
    if(!draw||displaySize.x<=0||displaySize.y<=0||!std::isfinite(displaySize.x)||
        !std::isfinite(displaySize.y)||!std::isfinite(nearClip)||nearClip<0)return;
    const bool unarmed = runtime.equippedWeapon < runtime.weapons.size() &&
        runtime.equippedWeapon < runtime.inventory.size() && runtime.inventory[runtime.equippedWeapon].owned &&
        runtime.weapons[runtime.equippedWeapon].nativeId == 0;
    if(!runtime.hud||runtime.paused||runtime.pauseMenu||runtime.health<=0||
        (!equippedWeaponIsFirearm(runtime)&&!unarmed)||
        (camera.manualAimActive&&camera.manualAimMode!=11))return;
    const auto* mission=runtime.missionEnvironment;
    if(mission&&(!mission->wantsHud||mission->widescreen||mission->cutscene.playing))return;
    if((runtime.playerAiming&&runtime.freeAimActive&&camera.freeAimActive&&storylandCanFreeAimPlayTestWeapon(runtime))||
        (runtime.manualAimActive&&camera.manualAimActive&&camera.manualAimMode==11)) {
        if(!firearmSight)return;
        const ImVec2 center(displaySize.x*.5f,displaySize.y*.5f);
        // VCS319370 binds hud:siteM16. Native3193CC uses half extents
        //14x14 in640x448, reducing horizontal extent to10.5 in widescreen.
        const float halfWidth=(displaySize.x/displaySize.y>1.5f?10.5f:14.0f)*displaySize.x/640.0f;
        const float halfHeight=14.0f*displaySize.y/448.0f;
        draw->AddImage(firearmSight,{center.x-halfWidth,center.y-halfHeight},
            {center.x+halfWidth,center.y+halfHeight},{0,0},{1,1},IM_COL32_WHITE);
        return;
    }
    if(!texture)return;
    StorylandTargetIndicator indicator;
    if(!storylandResolveTargetIndicator(runtime,pedestrians,indicator)||!indicator.positionValid)return;
    StorylandStoriesCamera view=camera;
    if(!runtime.crime.vcs&&view.aimLocked)view.target=view.aimTarget;
    const auto& eye=view.eye;
    const float dx=view.target.x-eye.x,dy=view.target.y-eye.y,dz=view.target.z-eye.z;
    const float length=std::hypot(dx,dy,dz);
    if(!std::isfinite(length)||length<=0)return;
    const float depth=((indicator.position.x-eye.x)*dx+(indicator.position.y-eye.y)*dy+
        (indicator.position.z-eye.z)*dz)/length;
    if(!std::isfinite(depth)||depth<=nearClip||depth>=2000)return;
    const float aspect=displaySize.x/displaySize.y;
    std::array<float,2> screen{};
    if(!storylandCameraScreenCoordinates(view,indicator.position,aspect,screen))return;
    const float ratio=runtime.targetIndicator.active?runtime.targetIndicator.healthRatio:
        indicator.maximumHealth>0?indicator.health/indicator.maximumHealth:0;
    const ImU32 color=storylandTargetIndicatorColor(runtime.crime.vcs,indicator.character,ratio);
    const ImVec2 scale(displaySize.x/480.0f,displaySize.y/272.0f);
    const auto coordinate=[&](ImVec2 point) {return ImVec2(point.x*scale.x,point.y*scale.y);};
    draw->PushClipRect({0,0},displaySize,true);
    for(const auto& quad:storylandTargetIndicatorQuads({screen[0],screen[1]},depth,aspect))
        draw->AddImageQuad(texture,coordinate(quad.corners[0]),coordinate(quad.corners[1]),
            coordinate(quad.corners[2]),coordinate(quad.corners[3]),
            quad.uv[0],quad.uv[1],quad.uv[2],quad.uv[3],color);
    draw->PopClipRect();
}

bool storylandHudObjectVisible(StorylandMissionEnvironment& mission,int16_t object,float meterValue) {
    const bool flashing=mission.flashingHudObject==object;
    if((flashing||(object==4&&meterValue<10.0f))&&(mission.nativeFrameCounter&8)==0)return false;
    if(flashing&&(object==3||object==4)&&meterValue>10.0f) {
        const size_t index=object==4?0:1;
        if(mission.hudFlashLastFrame[index]!=mission.nativeFrameCounter) {
            mission.hudFlashLastFrame[index]=mission.nativeFrameCounter;
            // Native31DBC0/31E550 reset each independent counter to50 only
            // after it becomes -1. Reissuing0269 does not reset the counter.
            if(--mission.hudFlashFrames[index]<0) {
                mission.hudFlashFrames[index]=50;mission.flashingHudObject=-1;
            }
        }
    }
    return true;
}

std::string storylandOnscreenTimerText(int32_t milliseconds) {
    char text[32];
    std::snprintf(text,sizeof(text),"%02d:%02d",(milliseconds/60000)%100,(milliseconds/1000)%60);
    return text;
}

std::string storylandOnscreenCounterText(int32_t value,int32_t maximum,bool money) {
    std::string text=money?"$":"";
    text+=std::to_string(maximum==-1?value:std::min(value,99));
    if(maximum!=-1)text+=" / "+std::to_string(std::min(uint32_t(maximum),99u));
    return text;
}

std::u16string storylandMissionAreaName(const StorylandMissionEnvironment& mission) {
    if (!mission.wantsHud || !mission.vcsAreaCaption.visible) {
        return {};
    }
    const std::string key = storylandMissionAreaNameKey(mission);
    return key.empty() ? std::u16string{} : storylandMissionFormattedText(mission, key);
}

void storylandDrawVcsAreaCaption(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& mission,ImVec2 displaySize) {
    const auto& caption = mission.vcsAreaCaption;
    if (!draw || !assets || displaySize.x <= 0 || displaySize.y <= 0 ||
        !mission.wantsHud || mission.widescreen || mission.cutscene.playing || mission.credits.active ||
        !caption.visible || caption.alpha == 0 || caption.displayedKey.empty()) {
        return;
    }
    if (!mission.helpMessage.empty() || !mission.bigMessageQueues[1].empty() ||
        !mission.bigMessageQueues[6].empty() || !mission.bigMessageQueues[2].empty()) {
        return;
    }
    const auto font = assets->fonts.find("fancytext");
    if (font == assets->fonts.end()) {
        return;
    }
    const ImVec2 scale(displaySize.x / 480.0f, displaySize.y / 272.0f);
    const ImVec4 clip(0, 234, 456, 234 + float(scriptMetric(font->second.height)));
    const auto text = storylandMissionFormattedText(mission, caption.displayedKey);
    const auto layout = storylandLayoutScriptText(font->second, text, 1.0f,
        displaySize.x / displaySize.y > 1.5f, true, 0, 0, clip, 0x14,
        IM_COL32(255, 255, 255, caption.alpha));
    draw->PushClipRect({clip.x * scale.x, clip.y * scale.y},
        {clip.z * scale.x, clip.w * scale.y}, true);
    drawScriptTextLayout(draw, assets, "fancytext", layout, scale);
    draw->PopClipRect();
}

void storylandDrawVcsVehicleCaption(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& mission,ImVec2 displaySize) {
    const auto& caption = mission.vcsVehicleCaption;
    if (!draw || !assets || displaySize.x <= 0 || displaySize.y <= 0 ||
        !mission.wantsHud || mission.widescreen || mission.cutscene.playing || mission.credits.active ||
        !storylandVcsHudComponentVisible(mission,7) || !caption.drawing ||
        caption.displayedKey.empty() || !mission.helpMessage.empty()) return;
    const auto font = assets->fonts.find("fancytext");
    if (font == assets->fonts.end()) return;
    const ImVec2 scale(displaySize.x / 480.0f,displaySize.y / 272.0f);
    const ImVec4 clip(0,210,456,210 + float(scriptMetric(font->second.height)));
    const auto text = storylandMissionFormattedText(mission,caption.displayedKey);
    const auto layout = storylandLayoutScriptText(font->second,text,1.0f,
        displaySize.x / displaySize.y > 1.5f,true,0,0,clip,0x14,IM_COL32_WHITE);
    draw->PushClipRect({clip.x * scale.x,clip.y * scale.y},{clip.z * scale.x,clip.w * scale.y},true);
    drawScriptTextLayout(draw,assets,"fancytext",layout,scale);
    draw->PopClipRect();
}

static StorylandScriptTextLayout storylandLayoutCreditsText(const StorylandRetailFont& font,
    const std::u16string& text,int32_t y,ImU32 color,bool vcs) {
    auto layout=storylandLayoutScriptText(font,text,.6f,false,true,vcs?306:240,y,
        {40,0,440,vcs?272.0f:448.0f},2,color);
    if(!vcs) {
        const int32_t previousHeight=layout.lineHeight;
        layout.lineHeight=scriptMetric(font.height*.7f);
        for(auto& glyph:layout.glyphs) {
            const int32_t row=previousHeight>0?int32_t(glyph.position.y-y)/previousHeight:0;
            glyph.position.y=float(y+row*layout.lineHeight);
            glyph.size.y=float(scriptMetric(glyph.glyph.height*.7f));
        }
    }
    return layout;
}

StorylandScriptTextLayout storylandLayoutNativeCreditLine(const StorylandRetailFont& font,
    const StorylandMissionEnvironment::CreditsLine& line,uint32_t scroll,bool vcs) {
    const int64_t y=int64_t(vcs?498:458)+line.offset-int64_t(scroll);
    if(y<=(vcs?-206:-118)||y>=(vcs?398:438))return {};
    return storylandLayoutCreditsText(font,line.text,int32_t(y-1),IM_COL32(220,220,220,255),vcs);
}

bool storylandPrepareNativeCredits(const StorylandGxtFile& gxt,const StorylandRetailHudAssets& assets,
    int32_t group,std::vector<StorylandMissionEnvironment::CreditsLine>& lines,std::string& error,bool vcs) {
    if(group<0||group>98) {error="Credit group is outside the native two-digit table names.";return false;}
    char tableName[16];std::snprintf(tableName,sizeof(tableName),"CRED%02d",group+1);
    const auto table=std::find_if(gxt.tables().begin(),gxt.tables().end(),
        [&](const auto& item){return item.name==tableName;});
    const auto font=assets.fonts.find("gametext");
    if(table==gxt.tables().end()||table->entries.empty()||table->entries.size()>(vcs?1024u:1000u)||font==assets.fonts.end()) {
        error="Native credits require their GXT table and game font.";return false;
    }
    std::vector<StorylandMissionEnvironment::CreditsLine> prepared;
    int32_t section=vcs?group+1:1,line=1,offset=0;
    for(size_t index=0;index<table->entries.size();++index) {
        char key[32];std::snprintf(key,sizeof(key),"CR%02d_%02d",section,line);
        const auto findKey=[&](const char* name)->size_t {
            for(size_t entry:table->entries)if(entry<gxt.entries().size()&&gxt.entries()[entry].key==name)
                return entry;
            return gxt.entries().size();
        };
        size_t entry=findKey(key);
        if(entry==gxt.entries().size()) {
            ++section;line=1;
            if(section>99) {error="Native credit section exceeds two digits.";return false;}
            std::snprintf(key,sizeof(key),"CR%02d_%02d",section,line);entry=findKey(key);
        }
        if(entry==gxt.entries().size()) {error="Native credit key sequence contains a missing line.";return false;}
        const auto decoded=gxt.text(entry);
        const auto layout=storylandLayoutCreditsText(font->second,decoded,0,IM_COL32(220,220,220,255),vcs);
        const int32_t height=std::max(1,layout.lineHeight)*int(std::max(size_t(1),layout.lineWidths.size()));
        prepared.push_back({key,offset,height,decoded});
        offset+=height+(vcs?0:14);
        if(offset>1000000) {error="Native credits exceed the bounded scroll range.";return false;}
        ++line;
    }
    lines=std::move(prepared);return true;
}

int storylandCompletionStage(const StorylandMissionEnvironment& mission) {
    int stage=0;
    for(int index=0;index<3;++index) {
        const auto found=mission.statistics.find("GameCompletionMilestone"+std::to_string(index));
        if(found!=mission.statistics.end()&&found->second!=0)stage=index+1;
    }
    const auto made=mission.statistics.find("ProgressMade");
    const auto total=mission.statistics.find("TotalProgressInGame");
    const double numerator=made==mission.statistics.end()?mission.progressMade:made->second;
    const double denominator=total==mission.statistics.end()?mission.progressTotal:total->second;
    if(denominator>0&&numerator>=denominator)stage=4;
    return stage;
}

void storylandDrawMissionPresentation(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    StorylandMissionEnvironment& mission,ImVec2 displaySize,const StorylandStoriesCamera* camera,
    const StorylandPlayTestRuntime* runtime,bool vcs) {
    if(!draw||!assets||displaySize.x<=0||displaySize.y<=0)return;
    const ImVec2 screenScale(displaySize.x/480.0f,displaySize.y/(vcs?272.0f:448.0f));
    const bool widescreen=displaySize.x/displaySize.y>1.5f;
    const auto coordinate=[&](float x,float y) {return ImVec2(x*screenScale.x,y*screenScale.y);};
    if(vcs&&mission.photographFlashUntil&&int32_t(mission.photographFlashUntil-mission.gameMilliseconds)>0)
        draw->AddRectFilled({0,0},displaySize,IM_COL32(255,255,255,180));
    if(mission.securityCamera) {
        // Retained CCTV switch; the host composes its visible scan filter here.
        draw->AddRectFilled({0,0},displaySize,IM_COL32(35,55,45,45));
        const float spacing=std::max(2.0f,displaySize.y/272.0f*3);
        const float offset=float(mission.gameMilliseconds%120u)/120.0f*spacing;
        for(float y=offset;y<displaySize.y;y+=spacing)
            draw->AddLine({0,y},{displaySize.x,y},IM_COL32(0,0,0,55),1);
    }
    if(!vcs&&mission.lcsExtendedWorld.liftCamera) {
        for(float y=0;y<displaySize.y;y+=4) {
            draw->AddRectFilled({0,y},{displaySize.x,y+1},IM_COL32(100,100,100,100));
            draw->AddRectFilled({0,y+2},{displaySize.x,y+3},IM_COL32(0,0,0,150));
        }
        const float scan=float(mission.gameMilliseconds&0x7FFu)*(displaySize.y+70)/2048-70;
        draw->AddRectFilled({0,scan},{displaySize.x,scan+70},IM_COL32(100,100,100,60));
        uint32_t noise=mission.nativeFrameCounter*1664525u+1013904223u;
        for(unsigned index=0;index<200;++index) {
            noise=noise*1664525u+1013904223u;
            const float x=float(noise%std::max(1u,uint32_t(displaySize.x)));
            noise=noise*1664525u+1013904223u;
            const float y=float(noise%std::max(1u,uint32_t(displaySize.y)));
            draw->AddRectFilled({x,y},{x+20,y+2},IM_COL32(255,255,255,64));
        }
    }
    if(vcs&&mission.drunkVisualIntensity) {
        const float phase=mission.drunkVisualPeriod?
            float(uint32_t(mission.gameMilliseconds-mission.drunkVisualStartedAt)%mission.drunkVisualPeriod)/
                float(mission.drunkVisualPeriod):0;
        const int alpha=std::clamp(int((.5f+.5f*std::sin(phase*6.283185307f))*
            std::min(55.0f,mission.drunkVisualAmplitude*4)),0,55);
        draw->AddRectFilled({0,0},displaySize,IM_COL32(80,95,110,alpha));
    }
    if(mission.credits.active) {
        const auto font=assets->fonts.find("gametext");
        if(font!=assets->fonts.end()) {
            draw->PushClipRect({0,0},displaySize,true);
            for(const auto& line:mission.credits.lines) {
                auto resolved=line;
                if(resolved.text.empty())resolved.text=storylandMissionFormattedText(mission,line.key);
                const auto layout=storylandLayoutNativeCreditLine(font->second,resolved,mission.credits.scroll,vcs);
                drawScriptTextLayout(draw,assets,"gametext",layout,screenScale);
            }
            draw->PopClipRect();
        }
    }
    if(!vcs)return;
    const auto clipBegin=[&](const ImVec4& rectangle) {
        draw->PushClipRect(coordinate(rectangle.x,rectangle.y),coordinate(rectangle.z,rectangle.w),true);
    };
    const bool manualOverlay=camera&&camera->manualAimActive&&
        (camera->manualAimMode==46||camera->manualAimMode==47||
            (runtime&&runtime->manualAimActive&&equippedWeaponIsFirearm(*runtime)));
    if(mission.scriptBinocularOverlay||manualOverlay) {
        // Native323750 selects the binocular raster for weapon39 or HUD+2C28.
        const int32_t overlayMode=mission.scriptBinocularOverlay?47:camera->manualAimMode;
        if(overlayMode==47) {
            // Native21F570..21F724 masks the optic independently of player aim.
            const float halfWidth=widescreen?170.0f:210.0f;
            draw->AddRectFilled(coordinate(0,0),coordinate(480,40),IM_COL32_BLACK);
            draw->AddRectFilled(coordinate(0,232),coordinate(480,272),IM_COL32_BLACK);
            draw->AddRectFilled(coordinate(0,0),coordinate(240-halfWidth,272),IM_COL32_BLACK);
            draw->AddRectFilled(coordinate(240+halfWidth,0),coordinate(480,272),IM_COL32_BLACK);
        }
        for(const auto& quad:storylandManualAimOverlay(mission.currentWeapon,overlayMode,widescreen)) {
            const auto texture=scriptHudTexture(assets,quad.texture);
            if(texture)draw->AddImageQuad(texture,coordinate(quad.corners[0].x,quad.corners[0].y),
                coordinate(quad.corners[1].x,quad.corners[1].y),coordinate(quad.corners[2].x,quad.corners[2].y),
                coordinate(quad.corners[3].x,quad.corners[3].y),quad.uv[0],quad.uv[1],quad.uv[2],quad.uv[3]);
        }
        if(overlayMode==47) {
            // Native323C58 uses the same scripted flag for the binocular heading ruler.
            const ImU32 rulerColor=IM_COL32(0,220,0,100);
            draw->AddRectFilled(coordinate(140,27),coordinate(340,29),rulerColor);
            for(unsigned index=0;index<9;++index) {
                const float x=140+float(index)*25;
                const bool shortTick=(index&1)!=0;
                draw->AddRectFilled(coordinate(x,shortTick?23.0f:18.0f),
                    coordinate(x+1,shortTick?33.0f:38.0f),rulerColor);
            }
            std::array<float,3> eye{},target{};
            bool cameraAvailable=false;
            if(camera) {
                eye={camera->eye.x,camera->eye.y,camera->eye.z};
                target={camera->target.x,camera->target.y,camera->target.z};
                cameraAvailable=true;
            } else if(mission.activeCameraVectors)
                cameraAvailable=mission.activeCameraVectors(eye,target);
            const float dx=target[0]-eye[0],dy=target[1]-eye[1];
            if(cameraAvailable&&std::isfinite(dx)&&std::isfinite(dy)&&std::hypot(dx,dy)>0) {
                const float bearing=std::atan2(-dx,dy)*57.295776f+180;
                for(unsigned heading=0;heading<360;heading+=45) {
                    float difference=bearing-float(heading);
                    if(difference>=315)difference-=360;
                    if(difference<=-20||difference>=20)continue;
                    const float x=240+difference*5;
                    draw->AddRectFilled(coordinate(x-2,18),coordinate(x+2,38),IM_COL32(0,220,0,150));
                    if(bearing>160&&bearing<200) {
                        // Native116CD8 draws radar sprite3 (radar_north) with half extents6.
                        const auto north=scriptHudTexture(assets,"radar_north");
                        if(north)draw->AddImage(north,coordinate(x-6,10),coordinate(x+6,22),
                            ImVec2(0,0),ImVec2(1,1),IM_COL32(0,255,0,150));
                    }
                    break;
                }
            }
        }
        if(camera&&overlayMode==7&&mission.currentWeapon==31&&camera->manualAimHit) {
            const auto texture=scriptHudTexture(assets,"laserdot");
            if(texture) {
                const auto& eye=camera->eye;const auto& target=camera->target;const auto& hit=camera->manualAimHitPoint;
                const float dx=target.x-eye.x,dy=target.y-eye.y,dz=target.z-eye.z;
                const float length=std::hypot(dx,dy,dz),horizontal=std::hypot(dx,dy);
                if(length>0&&horizontal>0) {
                    const float x=hit.x-eye.x,y=hit.y-eye.y,z=hit.z-eye.z;
                    const float depth=(x*dx+y*dy+z*dz)/length;
                    const float tangent=std::tan(camera->fieldOfView*.00872664626f);
                    if(depth>0&&tangent>0) {
                        const float side=(x*dy-y*dx)/horizontal;
                        const float up=(z*horizontal-(x*dx+y*dy)*dz/horizontal)/length;
                        const ImVec2 center(displaySize.x*.5f+side/depth/tangent*displaySize.y*.5f,
                            displaySize.y*.5f-up/depth/tangent*displaySize.y*.5f);
                        const float radius=std::clamp(80.0f/std::max(1.0f,depth),1.0f,8.0f)*screenScale.y;
                        draw->AddImage(texture,ImVec2(center.x-radius,center.y-radius),ImVec2(center.x+radius,center.y+radius),
                            ImVec2(0,0),ImVec2(1,1),IM_COL32(255,0,0,255));
                    }
                }
            }
        }
    }
    static constexpr const char* fonts[]={"fancytext","gametext","bigtext","pagertext"};
    for(const auto& panel:mission.scriptPanels) {
        if(!panel.visible||panel.width<=0||panel.height<=0)continue;
        const int32_t right=scriptCoordinateAdd(panel.x,panel.width),bottom=scriptCoordinateAdd(panel.y,panel.height);
        if(right<=panel.x||bottom<=panel.y)continue;
        const ImVec4 clip(float(panel.x),float(panel.y),float(right),float(bottom));
        draw->AddRectFilled(coordinate(float(panel.x),float(panel.y)),coordinate(float(right),float(bottom)),scriptColor(panel.backgroundColor));
        draw->AddRect(coordinate(float(panel.x)-2,float(panel.y)-2),coordinate(float(right)+2,float(bottom)+2),scriptColor(panel.borderColor),0,0,2*screenScale.y);
        clipBegin(clip);
        for(const auto& item:panel.items) {
            const int32_t x=scriptCoordinateAdd(panel.x,item.x),y=scriptCoordinateAdd(panel.y,item.y);
            if(item.sprite) {
                const auto texture=scriptHudTexture(assets,item.key);
                if(texture)draw->AddImage(texture,coordinate(float(x),float(y)),
                    coordinate(float(scriptCoordinateAdd(x,item.width)),float(scriptCoordinateAdd(y,item.height))),
                    ImVec2(0,0),ImVec2(1,1),scriptColor(item.color));
                continue;
            }
            if(item.font<0||item.font>=4)continue;
            const char* fontName=fonts[item.font];const auto font=assets->fonts.find(fontName);
            if(font==assets->fonts.end())continue;
            const auto text=storylandMissionFormattedText(mission,item.key,item.numbers);
            const auto layout=storylandLayoutScriptText(font->second,text,item.scale,widescreen,true,x,y,clip,item.alignment,scriptColor(item.color));
            if(item.highlight) {
                const float first=item.highlight==2?float(panel.x):float(x);
                const float last=item.highlight==2?float(right):float(x)+(layout.lineWidths.empty()?0:layout.lineWidths.front());
                draw->AddRectFilled(coordinate(first-2,float(y)-2),coordinate(last+2,float(y)+float(layout.lineHeight)+2),scriptColor(item.highlightColor));
            }
            drawScriptTextLayout(draw,assets,fontName,layout,screenScale);
        }
        draw->PopClipRect();
    }
    const auto gameFont=assets->fonts.find("gametext");
    if(gameFont!=assets->fonts.end()&&mission.scriptGlobalValue&&mission.wantsHud) {
        // CHud::DrawCounter/DrawClock, 0x3209D0..0x3214B4. The slot's
        // alpha belongs to the timer manager; the HUD uses its own opacity.
        const auto& font=gameFont->second;
        const float height=float(scriptMetric(font.height*.6f));
        const auto textLine=[&](const std::u16string& text,float right,float y,ImU32 color,bool proportional) {
            const ImVec4 clip(0,y,right,y+height);
            const auto layout=storylandLayoutScriptText(font,text,.6f,widescreen,proportional,0,0,clip,0x14,color);
            clipBegin(clip);drawScriptTextLayout(draw,assets,"gametext",layout,screenScale);draw->PopClipRect();
            return layout.lineWidths.empty()?0:layout.lineWidths.front();
        };
        const auto toUnits=[](const std::string& text) {return std::u16string(text.begin(),text.end());};
        const auto rgb=[](const auto& color) {return IM_COL32(color[0],color[1],color[2],255);};
        struct CounterFlash {bool active=false;uint32_t started=0;};
        static const StorylandMissionEnvironment* previous=nullptr;
        static uint32_t lastFrame=0;
        static std::array<CounterFlash,3> flashes{};
        if(previous!=&mission||mission.nativeFrameCounter<lastFrame)flashes={};
        previous=&mission;lastFrame=mission.nativeFrameCounter;
        std::array<bool,3> flashHidden{};
        for(size_t index=0;index<flashes.size();++index) {
            auto& flash=flashes[index];const bool active=mission.onscreenCounters[index].globalSlot>=0;
            if(active&&!flash.active)flash.started=mission.nativeFrameCounter;
            flash.active=active;
            flashHidden[index]=active&&uint32_t(mission.nativeFrameCounter-flash.started)<50&&
                (mission.nativeFrameCounter&4)==0;
        }
        int rows=0,headings=0;
        for(size_t index=0;index<mission.onscreenCounters.size();++index) {
            const auto& counter=mission.onscreenCounters[index];
            if(counter.globalSlot<0||counter.secondaryLabel.empty())continue;
            if(!flashHidden[index])textLine(storylandMissionFormattedText(mission,counter.secondaryLabel),454,
                float(100+31*headings),IM_COL32_WHITE,true);
            ++headings;
        }
        if(mission.onscreenTimer.globalSlot>=0) {
            const auto& timer=mission.onscreenTimer;
            if(!timer.warningFrames) {
                const float y=float(100+31*headings);
                const auto value=toUnits(storylandOnscreenTimerText(mission.scriptGlobalValue(timer.globalSlot)));
                const int width=textLine(value,456,y,rgb(timer.color),false);
                if(!timer.label.empty())textLine(storylandMissionFormattedText(mission,timer.label),
                    std::min(widescreen?415.0f:405.0f,452.0f-float(width)),y,rgb(timer.color),true);
            }
            ++rows;
        }
        for(size_t index=0;index<mission.onscreenCounters.size();++index) {
            auto& counter=mission.onscreenCounters[index];
            if(counter.globalSlot<0)continue;
            const float y=float(100+20*rows+31*headings);++rows;
            if(flashHidden[index]&&counter.secondaryLabel.empty())continue;
            // 0x3962C4 persists this unsigned clamp as part of formatting.
            if(counter.maximum!=-1)counter.maximum=int32_t(std::min(uint32_t(counter.maximum),99u));
            const auto value=storylandOnscreenCounterText(mission.scriptGlobalValue(counter.globalSlot),counter.maximum,counter.money);
            float occupied=0;
            if(!counter.type)occupied=float(textLine(toUnits(value),456,y,rgb(counter.color),false))+4;
            else {
                const float left=widescreen?426.0f:414.0f;
                occupied=widescreen?35.0f:47.0f;
                // Native atoi is followed by a signed halfword conversion;
                // money-prefixed bars therefore have zero fill.
                const long long parsed=std::strtoll(value.c_str(),nullptr,10);
                const uint16_t bits=uint16_t(uint64_t(parsed));
                int16_t percentage;std::memcpy(&percentage,&bits,sizeof(percentage));
                draw->AddRectFilled(coordinate(left-1,y),coordinate(456,y+13),IM_COL32_BLACK);
                draw->AddRectFilled(coordinate(left,y+1),coordinate(455,y+12),rgb(counter.background));
                draw->AddRectFilled(coordinate(left,y+1),coordinate(left+(widescreen?29.0f:41.0f)*float(percentage)*.01f,y+12),rgb(counter.color));
            }
            if(!counter.label.empty())textLine(storylandMissionFormattedText(mission,counter.label),
                std::min(widescreen?415.0f:405.0f,456.0f-occupied),y,rgb(counter.color),true);
        }
    }
    drawScriptFrameText(draw, assets, mission, displaySize, false);
    // Big print queues are independent of subtitle/HUD visibility. Each native
    // style owns its active front entry, including mission result banners.
    for(size_t style=0;style<mission.bigMessageQueues.size();++style) {
        const auto& queue=mission.bigMessageQueues[style];
        if(queue.empty()||!queue.front().remaining)continue;
        const auto& message=queue.front();
        if(vcs&&style==1) {
            const auto font=assets->fonts.find("fancytext");
            if(font==assets->fonts.end()||!assets->sprites.count("fsfonts:fancytext"))continue;
            const auto text=storylandMissionFormattedText(mission,message.key,message.numbers,message.stringKey);
            const int32_t top=268-scriptMetric(font->second.height*1.2f);
            const ImVec4 clip(0,0,460,272);
            const auto layout=storylandLayoutScriptText(font->second,text,1.2f,widescreen,true,
                0,top,clip,4,IM_COL32(220,172,2,255),false,false,true);
            auto shadow=layout;
            for(auto& glyph:shadow.glyphs)glyph.color=IM_COL32(0,0,0,255);
            draw->PushClipRect({2*screenScale.x,2*screenScale.y},
                {467*screenScale.x,274*screenScale.y},true);
            drawScriptTextLayout(draw,assets,"fsfonts:fancytext",shadow,screenScale,{2,2});
            draw->PopClipRect();
            draw->PushClipRect({0,0},{465*screenScale.x,272*screenScale.y},true);
            drawScriptTextLayout(draw,assets,"fsfonts:fancytext",layout,screenScale,{});
            draw->PopClipRect();
            continue;
        }
        const auto utf8=scriptTextUtf8(storylandMissionFormattedText(mission,message.key,message.numbers,message.stringKey));
        const bool result=message.key=="M_PASS"||message.key=="M_PASSN"||message.key=="M_FAIL";
        const char* fontName=style==7?"gametext":"bigtext";
        const float height=(style==7?16.0f:result?25.0f:21.0f)*screenScale.y;
        const float top=(style==7?210.0f:style==4?170.0f:style==5?45.0f:110.0f)*screenScale.y;
        const float alpha=std::min(1.0f,float(message.remaining)/300.0f);
        const ImU32 color=message.key=="M_FAIL"?IM_COL32(235,80,100,int(alpha*255)):
            IM_COL32(237,130,180,int(alpha*255));
        storylandDrawPlayTestText(draw,assets,ImVec2(displaySize.x*.5f+screenScale.x,top+screenScale.y),
            IM_COL32(0,0,0,int(alpha*255)),utf8.c_str(),height,displaySize.x*.94f,fontName,true);
        if(vcs&&(message.key=="M_PASS"||message.key=="M_PASSN"))
            storylandDrawUiGlow(ImVec2(displaySize.x*.5f,top),ImVec2(screenScale.x*1.25f,screenScale.y*1.25f),color,
                [&](ImVec2 point,ImU32 tint) {
                    storylandDrawPlayTestText(draw,assets,point,tint,utf8.c_str(),height,displaySize.x*.94f,fontName,true);
                });
        storylandDrawPlayTestText(draw,assets,ImVec2(displaySize.x*.5f,top),color,utf8.c_str(),height,displaySize.x*.94f,fontName,true);
    }
}

static void drawScriptFrameText(ImDrawList* draw, const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& mission, ImVec2 displaySize, bool late) {
    if (!draw || !assets || displaySize.x <= 0 || displaySize.y <= 0) {
        return;
    }
    static constexpr const char* fontNames[]={"fancytext","gametext","bigtext","pagertext"};
    const ImVec2 screenScale(displaySize.x / 480.0f, displaySize.y / 272.0f);
    const bool widescreen = displaySize.x / displaySize.y > 1.5f;
    for (const auto& text : mission.scriptFrameText) {
        if(text.font>=std::size(fontNames))continue;
        const auto* fontName=fontNames[text.font];
        const auto font=assets->fonts.find(fontName);
        if(font==assets->fonts.end())continue;
        if (((text.flags & 0x20) != 0) != late) {
            continue;
        }
        // Native 0x321758 draws after the HUD. Its clip is [x,x+wrap], and it
        // preserves authored X rather than correcting the normal right edge.
        const float left = late ? float(text.x) : 0.0f;
        const ImVec4 clip(left, 0, left + text.wrap, 272);
        if (clip.z <= clip.x) {
            continue;
        }
        const int32_t alignment = (text.flags & 2) ? 2 : (text.flags & 64) ? 4 : (text.flags & 1) ? 1 : 0;
        const auto layout = storylandLayoutScriptText(font->second, text.text, text.scaleY, widescreen,
            (text.flags & 16) != 0, text.x, text.y, clip, alignment, scriptColor(text.color), !late);
        draw->PushClipRect(ImVec2(clip.x * screenScale.x, clip.y * screenScale.y),
            ImVec2(clip.z * screenScale.x, clip.w * screenScale.y), true);
        drawScriptTextLayout(draw, assets, fontName, layout, screenScale);
        draw->PopClipRect();
    }
}

ImTextureID storylandRetailSpriteTexture(const StorylandRetailHudAssets* assets,const std::string& name) {
    return scriptHudTexture(assets,name);
}
