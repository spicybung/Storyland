#pragma once
#include "src/leeds/leeds_archive.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

inline std::vector<bool> storylandSelectNearbyWorldSectors(
    const std::vector<StorylandWorldSector>& worldSectors,
    const std::array<uint32_t,256>& worldSwapGroups,
    float x,float y,bool scripted,int radius) {
    std::vector<bool> result(worldSectors.size(),false);
    if(!std::isfinite(x)||!std::isfinite(y))return result;
    // 3 neighbouring rings cover the Intro camera's long pan. Ordinary
    // gameplay still uses the retail single sector and its regular LODs.
    radius=std::clamp(radius,0,3);
    const double rowPosition=std::floor((double(y)+2000.0)/108.25);
    if(rowPosition < -double(radius) || rowPosition >= 36.0+double(radius))return result;
    const int centerRow=int(rowPosition);
    std::array<int,7> columns{};
    columns.fill(INT_MIN);
    for(int row=std::max(0,centerRow-radius);row<=std::min(35,centerRow+radius);++row){
        const double columnPosition=std::floor((double(x)+2400.0)/125.0+(row&1)*.5);
        if(columnPosition < -double(radius) || columnPosition > 65535.0+double(radius))continue;
        columns[size_t(row-centerRow+radius)]=int(columnPosition);
    }
    // A native sector may have conditional replacements (garage/gate/scene
    // swaps). Prefer the active replacement, otherwise draw its base entry.
    std::set<std::pair<uint32_t,uint32_t>> swappedCells;
    const auto selected=[&](const StorylandWorldSector& sector){
        if(int(sector.sectorY)<centerRow-radius||int(sector.sectorY)>centerRow+radius||
           sector.sectorY>=36u)return false;
        const int centerColumn=columns[size_t(int(sector.sectorY)-centerRow+radius)];
        if(centerColumn==INT_MIN)return false;
        return int64_t(sector.sectorX)>=int64_t(centerColumn)-radius&&
               int64_t(sector.sectorX)<=int64_t(centerColumn)+radius;
    };
    for(size_t index=0;index<worldSectors.size();++index){
        const auto& sector=worldSectors[index];
        if(!selected(sector)||!sector.conditional||!scripted||
           worldSwapGroups[sector.swapGroup]!=sector.swapState)continue;
        result[index]=true;
        swappedCells.emplace(sector.sectorX,sector.sectorY);
    }
    for(size_t index=0;index<worldSectors.size();++index){
        const auto& sector=worldSectors[index];
        if(!selected(sector)||sector.conditional||
            swappedCells.count({sector.sectorX,sector.sectorY}))continue;
        result[index]=true;
    }
    return result;
}

