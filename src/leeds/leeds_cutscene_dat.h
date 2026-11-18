#pragma once
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

struct StorylandCutsceneDatKey {
    float time = 0;
    std::array<float,3> value{}, incoming{}, outgoing{};
};

inline std::string storylandCutsceneDatTrim(const std::string& input) {
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    return input.substr(start,input.find_last_not_of(" \t\r\n")-start+1);
}

inline bool storylandCutsceneDatNumbers(const std::string& line,
    std::vector<float>& numbers) {
    numbers.clear();
    const char* position=line.c_str();
    const char* finish=position+line.size();
    while(position<finish) {
        while(position<finish&&(*position==' '||*position=='\t'||*position==',')) ++position;
        if(position==finish) break;
        char* numberEnd=nullptr;
        errno=0;
        const float number=std::strtof(position,&numberEnd);
        if(numberEnd==position||errno==ERANGE||!std::isfinite(number))return false;
        position=numberEnd;
        if(position<finish&&(*position=='f'||*position=='F'))++position;
        if(position<finish&&*position!=','&&*position!=' '&&*position!='\t')return false;
        numbers.push_back(number);
    }
    return !numbers.empty();
}

inline bool storylandDecodeCutsceneDat(const std::vector<uint8_t>& source,
    std::array<std::vector<StorylandCutsceneDatKey>,4>& splines,
    std::string& error) {
    for(auto& spline:splines)spline.clear();
    if(source.empty()||source.size()>4u*1024u*1024u) {
        error="Native cutscene DAT has an invalid file size.";
        return false;
    }
    std::string content(reinterpret_cast<const char*>(source.data()),source.size());
    const size_t terminator=content.find('\0');
    if(terminator!=std::string::npos)content.resize(terminator);
    std::istringstream sections(content);
    std::string section;
    for(size_t spline=0;spline<4;++spline) {
        if(!std::getline(sections,section,';')) {
            error="Native cutscene DAT is missing camera spline " + std::to_string(spline+1) + ".";
            return false;
        }
        std::istringstream lines(section);
        std::string line;
        size_t expected=0;
        bool hasCount=false;
        std::vector<float> numbers;
        while(std::getline(lines,line)) {
            const auto comment=line.find("//");
            if(comment!=std::string::npos)line.resize(comment);
            line=storylandCutsceneDatTrim(line);
            if(line.empty())continue;
            if(!hasCount) {
                if(!storylandCutsceneDatNumbers(line,numbers)||numbers.size()!=1||
                    numbers[0]<1||numbers[0]>4096||std::floor(numbers[0])!=numbers[0]) {
                    error="Native cutscene DAT spline has an invalid key count.";
                    return false;
                }
                expected=size_t(numbers[0]);hasCount=true;
                continue;
            }
            const size_t dimensions=spline<2?1:3;
            if(!storylandCutsceneDatNumbers(line,numbers)||numbers.size()!=1+dimensions*3) {
                error="Native cutscene DAT spline key has the wrong number of values.";
                return false;
            }
            if(splines[spline].size()>=expected||numbers[0]<0||numbers[0]>3600||
                (!splines[spline].empty()&&numbers[0]<splines[spline].back().time)) {
                error="Native cutscene DAT spline key time is invalid.";
                return false;
            }
            StorylandCutsceneDatKey key;
            key.time=numbers[0];
            for(size_t axis=0;axis<dimensions;++axis) {
                key.value[axis]=numbers[1+axis];
                key.incoming[axis]=numbers[1+dimensions+axis];
                key.outgoing[axis]=numbers[1+dimensions*2+axis];
            }
            splines[spline].push_back(key);
        }
        if(!hasCount||splines[spline].size()!=expected) {
            error="Native cutscene DAT spline key count does not match the declared count.";
            return false;
        }
    }
    std::string trailing;
    while(std::getline(sections,trailing,';')) {
        if(!storylandCutsceneDatTrim(trailing).empty()) {
            error="Native cutscene DAT contains unexpected camera data.";
            return false;
        }
    }
    error.clear();
    return true;
}
