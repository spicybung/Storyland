#pragma once
#include "src/storyland/storyland_radio_stations.h"
#include <string>
#include <cctype>
#include <array>

inline std::string storylandHudKey(const std::string& value) {
    std::string result;
    for(unsigned char character:value)if(std::isalnum(character))result.push_back(char(std::tolower(character)));
    return result;
}
inline std::string storylandRadioLogoKey(std::string value) {
    value=storylandHudKey(value);
    for(int step=0;step<3;++step)for(const auto& prefix:{std::string("radio"),std::string("logo"),std::string("station"),std::string("icon")}) {
        if(value.size()>prefix.size()&&value.compare(0,prefix.size(),prefix)==0)value.erase(0,prefix.size());
        if(value.size()>prefix.size()&&value.compare(value.size()-prefix.size(),prefix.size(),prefix)==0)value.resize(value.size()-prefix.size());
    }
    if(value=="usertracks")return value;
    if(!storylandIsRadioStation(value))return {};
    if(value=="head")return "headradio";
    if(value=="lips")return "lips106";
    if(value=="class"||value=="classical"||value=="doublecleffm")return "doubleclef";
    if(value=="game"||value=="gamefm"||value=="jam"||value=="thelibertyjam")return "libertyjam";
    if(value=="world"||value=="radiodelmundo")return "delmundo";
    if(value=="msx")return "msx98";
    if(value=="espant"||value=="radioespantoso")return "espantoso";
    if(value=="emotion983")return "emotion";
    if(value=="fresh105"||value=="freshfm")return "fresh";
    if(value=="paradisefm")return "paradise";
    if(value=="wave103")return "wave";
    if(value=="risefm")return "rise";
    if(value=="flashfm")return "flash";
    if(value=="flashbackfm")return "flashback";
    if(value=="chat"||value=="libertycityfreeradio")return "lcfr";
    return value;
}
inline std::string storylandVcsRadioSpriteName(const std::string& station) {
    if(storylandRadioLogoKey(station)=="usertracks")return "radio:usertracks";
    const auto info=storylandRadioStationInfo(station,true);
    if(info.key.empty())return {};
    const int channel=storylandRadioStationOrder(info.key,true);
    if(channel<0||channel>=9)return {};
    return "radio_"+std::to_string(channel+1);
}
inline std::string storylandWeaponHudKey(const std::string& name) {
    auto key=storylandHudKey(name);
    if(key=="pistol")return "colt45";
    if(key=="rlaunch"||key=="rocketlauncher"||key=="rocketla"||key=="rpg")return "rocket";
    if(key=="unarmed"||key=="fists")return "fist";
    return key;
}
inline int storylandRadarTileIndex(std::string name) {
    name=storylandHudKey(name);
    if(name.size()<6||name.compare(0,5,"radar")!=0)return -1;
    int index=0;
    for(size_t cursor=5;cursor<name.size();++cursor) {
        if(!std::isdigit(static_cast<unsigned char>(name[cursor])))return -1;
        index=index*10+name[cursor]-'0';if(index>255)return -1;
    }
    return index;
}
inline bool storylandHudDictionaryWanted(const std::string& name) {
    const auto key=storylandHudKey(name);
    if(storylandRadarTileIndex(key)>=0)return true;
    static const char* names[]={"hud","emphud","generic","particle","fonts","fsfonts","front","frontend","menu","radar","radio","weapons","weapon",
        "colt45","pistol","python","deagle","uzi","mp5","mp5lng","tec9","ingram","ak47","m4","m16","ruger","shotgun",
        "spas12","stubby","sniper","laser","laserscope","rlaunch","rocket","minigun","m60","molotov","grenade","teargas"};
    for(const auto* value:names)if(key==value)return true;
    return false;
}
