#include "src/graphics/storyland_native_graphics.h"
#include <algorithm>
#include <cstring>
#include <cmath>
namespace {
uint32_t read32(const std::vector<uint8_t>& bytes,size_t offset) {
    if(offset>bytes.size()||bytes.size()-offset<4) return 0;
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset+1])<<8 |
        uint32_t(bytes[offset+2])<<16 | uint32_t(bytes[offset+3])<<24;
}
int read16(const std::vector<uint8_t>& bytes,size_t offset) {
    return int16_t(uint16_t(bytes[offset]) | uint16_t(bytes[offset+1])<<8);
}
}
bool StorylandNativeTimecycle::load(const std::vector<uint8_t>& dtz,bool vcs,std::string& error) {
    loaded=false;
    const size_t base=read32(dtz,vcs?0xB4:0xB0);
    constexpr size_t samples=24*8, start=0x280;
    const size_t cloudStart=start+33*samples+4*samples+(vcs?2*samples:0)+samples;
    const size_t requiredSize=vcs?0x3280u:cloudStart+9*samples;
    if(base<0xC0||base>dtz.size()||requiredSize>dtz.size()-base) {
        error="Native timecycle tables are missing or truncated.";return false;
    }
    unsigned valid=0;
    const auto readFloat=[&](size_t offset) {
        const uint32_t bits=read32(dtz,offset);
        float value=0;std::memcpy(&value,&bits,sizeof(value));
        return std::isfinite(value)?value:0.0f;
    };
    std::array<std::array<StorylandNativeTimecycleRow,24>,8> decoded{};
    for(unsigned weather=0;weather<8;++weather) for(unsigned hour=0;hour<24;++hour) {
        const size_t index=hour*8+weather;
        auto& entry=decoded[weather][hour];
        const auto color=[&](size_t channel) {
            return std::array<uint8_t,3>{dtz[base+start+channel*samples+index],
                dtz[base+start+(channel+1)*samples+index],dtz[base+start+(channel+2)*samples+index]};
        };
        entry.ambient=color(0);entry.objectAmbient=color(3);entry.directional=color(12);
        entry.ambientBlur=color(6);entry.objectAmbientBlur=color(9);
        entry.skyTop=color(15);entry.skyBottom=color(18);entry.sunCore=color(21);entry.sunCorona=color(24);
        entry.sunSize=float(dtz[base+start+27*samples+index])/10.0f;
        entry.spriteSize=float(dtz[base+start+28*samples+index])/10.0f;
        entry.spriteBrightness=float(dtz[base+start+29*samples+index])/10.0f;
        entry.shadowStrength=dtz[base+start+30*samples+index];
        entry.lightShadowStrength=dtz[base+start+31*samples+index];
        entry.poleShadowStrength=dtz[base+start+32*samples+index];
        entry.farClip=float(read16(dtz,base+start+33*samples+index*2));
        entry.fogStart=float(read16(dtz,base+start+35*samples+index*2));
        if(vcs) {
            entry.radiosityIntensity=float(dtz[base+0x1E40+index]);
            entry.radiosityLimit=float(dtz[base+0x1F00+index]);
            entry.frameGlowIntensity=readFloat(base+0x2C80+index*4);
            entry.frameGlowRadius=readFloat(base+0x2F80+index*4);
            entry.frameFilter={dtz[base+0x2740+index],dtz[base+0x2800+index],dtz[base+0x28C0+index]};
        }
        const auto cloud=[&](size_t channel) {
            return std::array<uint8_t,3>{dtz[base+cloudStart+channel*samples+index],
                dtz[base+cloudStart+(channel+1)*samples+index],dtz[base+cloudStart+(channel+2)*samples+index]};
        };
        entry.lowCloud=cloud(0);entry.cloudTop=cloud(3);entry.cloudBottom=cloud(6);
        if(entry.farClip>=50&&entry.farClip<=20000&&entry.fogStart>=-20000&&entry.fogStart<=entry.farClip) ++valid;
    }
    if(valid<180) { error="Native timecycle distances failed validation.";return false; }
    rows=std::move(decoded);vcsGame=vcs;loaded=true;error.clear();return true;
}
const StorylandNativeTimecycleRow& StorylandNativeTimecycle::row(unsigned hour,unsigned weather) const {
    return rows[std::min(weather,7u)][hour%24];
}
std::vector<std::array<uint8_t,3>> storylandNativeClothingPalette(const std::vector<uint8_t>& dtz,bool vcs) {
    if(!vcs) return {};
    const size_t base=read32(dtz,0x90);
    if(base<0xC0||base>dtz.size()||768>dtz.size()-base) return {};
    std::vector<std::array<uint8_t,3>> result;
    for(size_t index=0;index<128;++index) result.push_back({dtz[base+index*3],dtz[base+index*3+1],dtz[base+index*3+2]});
    return result;
}
std::vector<std::array<uint8_t,3>> storylandNativeVehiclePalette(const std::vector<uint8_t>& dtz,bool vcs) {
    size_t base=read32(dtz,vcs?0x90:0x78);
    const size_t stride=vcs?3:4;
    base+=vcs?128*3:4;
    const size_t count=vcs?128:256;
    if(base<0xC0||base>dtz.size()||count*stride>dtz.size()-base) return {};
    std::vector<std::array<uint8_t,3>> result;
    for(size_t index=0;index<count;++index) result.push_back({dtz[base+index*stride],dtz[base+index*stride+1],dtz[base+index*stride+2]});
    return result;
}
int storylandNativeClothingSlot(uint32_t rgba) {
    // VCS uses these markers for authored clothing and skin palette slots.
    constexpr uint32_t markers[]={0x003CFF00u,0x00AF00FFu,0x003C00FFu,0x00FF003Cu,0x00AF003Cu};
    for(int slot=0;slot<5;++slot) if((rgba&0xFFFFFFu)==markers[slot]) return slot;
    return -1;
}
uint32_t storylandNativePedMaterialColor(uint32_t rgba,
    const std::array<uint32_t,6>& clothingColors) {
    const int slot=storylandNativeClothingSlot(rgba);
    if(slot<0)return rgba;
    return (rgba&0xFF000000u)|(clothingColors[size_t(slot)]&0x00FFFFFFu);
}
// Both consoles keep red in the low five bits and alpha in the top bit.
std::array<uint8_t,4> storylandGraphicsDecode5551(uint16_t packed,StorylandGraphicsPlatform platform) {
    if(platform!=StorylandGraphicsPlatform::Ps2&&platform!=StorylandGraphicsPlatform::Psp) return {255,255,255,255};
    if(platform==StorylandGraphicsPlatform::Psp) {
        const auto expand=[](uint16_t channel) {
            channel&=31u;
            return uint8_t((channel<<3u)|(channel>>2u));
        };
        return {expand(packed),expand(packed>>5u),expand(packed>>10u),
            uint8_t((packed&0x8000u)?255:0)};
    }
    return {uint8_t((packed&31u)*255u/31u),uint8_t(((packed>>5u)&31u)*255u/31u),
        uint8_t(((packed>>10u)&31u)*255u/31u),uint8_t((packed&0x8000u)?255:0)};
}
