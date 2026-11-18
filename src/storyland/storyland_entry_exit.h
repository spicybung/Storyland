#pragma once
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

struct StorylandEntryExit {
    StorylandNativePoint entrance,exit;
    float entranceHeading=0,exitHeading=0,radiusX=1,radiusY=1,radiusZ=2;
    int sourceArea=0,destinationArea=0;
    uint32_t flags=0;
    int hourOn=0,hourOff=0;
    std::string name;
    bool enabled=true;
};

inline bool storylandEntryExitOpen(const StorylandEntryExit& entry,float hour,bool reverse=false) {
    if(!entry.enabled || (entry.flags&128u) || (reverse&&(entry.flags&2048u)))return false;
    if(!std::isfinite(hour))return false;
    if(entry.hourOn==entry.hourOff)return true;
    const float current=std::fmod(std::fmod(hour,24.0f)+24.0f,24.0f);
    if(entry.hourOn<entry.hourOff)return current>=entry.hourOn&&current<entry.hourOff;
    return current>=entry.hourOn||current<entry.hourOff;
}

inline bool storylandEntryExitContains(const StorylandEntryExit& entry,
    const StorylandNativePoint& point,bool reverse=false) {
    const StorylandNativePoint& position=reverse?entry.exit:entry.entrance;
    const float angle=(reverse?entry.exitHeading:entry.entranceHeading)*0.01745329251994329577f;
    const float dx=point.x-position.x,dy=point.y-position.y;
    return std::fabs(dx*std::cos(angle)+dy*std::sin(angle))<=entry.radiusX&&
        std::fabs(-dx*std::sin(angle)+dy*std::cos(angle))<=entry.radiusY&&
        std::fabs(point.z-position.z)<=std::min(entry.radiusZ,3.5f);
}

inline std::vector<StorylandEntryExit> storylandReadEntryExits(std::istream& input,bool standalone=false) {
    std::vector<StorylandEntryExit> result;
    bool section=standalone;
    std::string line;
    while(std::getline(input,line)) {
        bool quoted=false;
        for(size_t index=0;index<line.size();++index) {
            if(line[index]=='"')quoted=!quoted;
            else if(!quoted&&(line[index]=='#'||line[index]==';')) {line.resize(index);break;}
        }
        std::replace(line.begin(),line.end(),',',' ');
        std::istringstream word(line);
        std::string tag;
        word>>tag;
        std::transform(tag.begin(),tag.end(),tag.begin(),[](unsigned char value){return char(std::tolower(value));});
        if(tag=="enex") {section=true;continue;}
        if(tag=="end") {section=false;continue;}
        if(!section||tag.empty())continue;
        StorylandEntryExit entry;
        std::istringstream row(line);
        if(!(row>>entry.entrance.x>>entry.entrance.y>>entry.entrance.z
            >>entry.entranceHeading>>entry.radiusX>>entry.radiusY>>entry.radiusZ
            >>entry.exit.x>>entry.exit.y>>entry.exit.z>>entry.exitHeading
            >>entry.destinationArea>>entry.flags))continue;
        row>>std::ws;
        if(row.peek()=='"')row>>std::quoted(entry.name);
        else row>>entry.name;
        int sky=0,pedestrians=0,hourOn=0,hourOff=0;
        if(row>>sky>>pedestrians>>hourOn>>hourOff) {
            if(hourOn<0||hourOn>24||hourOff<0||hourOff>24)continue;
            entry.hourOn=hourOn%24;
            entry.hourOff=hourOff%24;
        }
        if(entry.radiusZ==0.0f)entry.radiusZ=2.0f;
        const float values[]={entry.entrance.x,entry.entrance.y,entry.entrance.z,
            entry.exit.x,entry.exit.y,entry.exit.z,entry.entranceHeading,
            entry.exitHeading,entry.radiusX,entry.radiusY,entry.radiusZ};
        if(!std::all_of(std::begin(values),std::end(values),[](float value) {
            return std::isfinite(value)&&std::fabs(value)<100000.0f;
        }))continue;
        if(entry.radiusX<=0||entry.radiusY<=0||entry.radiusZ<=0||entry.radiusX>50||
            entry.radiusY>50||entry.radiusZ>50||entry.destinationArea<0||entry.destinationArea>255)continue;
        if(result.size()>=4096)break;
        result.push_back(std::move(entry));
    }
    return result;
}

struct StorylandEntryExitWarp {
    StorylandNativePoint position{};
    float heading=0;
    int area=0;
};

inline bool storylandResolveEntryExitWarp(const StorylandEntryExit& entry,
    bool returning,StorylandEntryExitWarp& warp) {
    const auto& position=returning?entry.entrance:entry.exit;
    const float heading=returning?entry.entranceHeading:entry.exitHeading;
    const int area=returning?entry.sourceArea:entry.destinationArea;
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||
       !std::isfinite(position.z)||!std::isfinite(heading)||
       area<0||area>255)return false;
    warp.position=position;warp.heading=heading;warp.area=area;
    return true;
}

inline void storylandResolveEntryExitAreas(std::vector<StorylandEntryExit>& entries) {
    const auto near=[](const StorylandNativePoint& first,const StorylandNativePoint& second) {
        return std::hypot(first.x-second.x,first.y-second.y,first.z-second.z)<3.0f;
    };
    for(size_t index=0;index<entries.size();++index) {
        auto& entry=entries[index];
        if(entry.name.empty()||entry.sourceArea!=0||entry.destinationArea!=0)continue;
        for(size_t other=0;other<entries.size();++other) {
            if(index==other||entries[other].name!=entry.name||entries[other].destinationArea<=0)continue;
            if(near(entry.entrance,entries[other].exit)) {
                entry.sourceArea=entries[other].destinationArea;
                break;
            }
        }
    }
}

inline void storylandAppendEntryExits(std::vector<StorylandEntryExit>& destination,
    const std::vector<StorylandEntryExit>& entries) {
    for(const auto& entry:entries) {
        const auto samePoint=[](StorylandNativePoint first,StorylandNativePoint second) {
            return std::hypot(first.x-second.x,first.y-second.y,first.z-second.z)<.01f;
        };
        const bool exists=std::any_of(destination.begin(),destination.end(),[&](const auto& prior) {
            return prior.sourceArea==entry.sourceArea&&prior.destinationArea==entry.destinationArea&&
                samePoint(prior.entrance,entry.entrance)&&samePoint(prior.exit,entry.exit);
        });
        if(!exists&&destination.size()<4096)destination.push_back(entry);
    }
    storylandResolveEntryExitAreas(destination);
}

template<class Archive>
void storylandLoadArchivedEntryExits(const Archive& archive,std::vector<StorylandEntryExit>& destination) {
    const auto& entries=archive.dirEntries();
    for(size_t index=0;index<entries.size();++index) {
        std::string extension=std::filesystem::path(entries[index].name).extension().string();
        for(char& character:extension)character=char(std::tolower(static_cast<unsigned char>(character)));
        if(extension!=".enex"&&extension!=".ipl")continue;
        if(entries[index].byteLength>16u*1024u*1024u)continue;
        std::vector<uint8_t> bytes;
        std::string error;
        if(!archive.extractDirEntryBytes(index,bytes,error))continue;
        while(!bytes.empty()&&bytes.back()==0)bytes.pop_back();
        if(std::find(bytes.begin(),bytes.end(),uint8_t(0))!=bytes.end())continue;
        std::istringstream input(std::string(bytes.begin(),bytes.end()));
        storylandAppendEntryExits(destination,storylandReadEntryExits(input,extension==".enex"));
    }
}

// An LVZ+IMG area can carry an authored text IPL/ENEX alongside model
// resources. The DTZ directory and the discovered files are not the only
// available containers; scan the actual opened area archives as well.
template<class Browser>
size_t storylandLoadAreaEntryExits(const Browser& archive,
    std::vector<StorylandEntryExit>& destination) {
    const size_t originalCount=destination.size();
    const auto& entries=archive.entries();
    for(size_t index=0;index<entries.size();++index) {
        std::string extension=std::filesystem::path(entries[index].name).extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char character) {
            return char(std::tolower(character));
        });
        if(extension!=".ipl"&&extension!=".enex")continue;
        if(entries[index].byteSize==0||entries[index].byteSize>16u*1024u*1024u)continue;
        std::vector<uint8_t> bytes;
        std::string error;
        if(!archive.extractEntryBytes(index,bytes,error))continue;
        while(!bytes.empty()&&bytes.back()==0)bytes.pop_back();
        if(bytes.empty()||bytes.size()>16u*1024u*1024u||
            std::find(bytes.begin(),bytes.end(),uint8_t(0))!=bytes.end())continue;
        std::istringstream stream(std::string(bytes.begin(),bytes.end()));
        storylandAppendEntryExits(destination,storylandReadEntryExits(stream,extension==".enex"));
    }
    return destination.size()-originalCount;
}
