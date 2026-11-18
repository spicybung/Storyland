#pragma once

#include "src/leeds/leeds_archive.h"
#include <algorithm>
#include <vector>

inline void storylandCollectSceneMaterials(const StorylandWorldMesh& mesh,
    std::vector<uint32_t>& materials) {
    materials.clear();
    for(const auto& triangle:mesh.triangles) {
        const auto position=std::lower_bound(materials.begin(),materials.end(),triangle.textureId);
        if(position==materials.end()||*position!=triangle.textureId)
            materials.insert(position,triangle.textureId);
    }
}

template<class Visit>
bool storylandVisitSceneMaterialRuns(const StorylandWorldMesh& mesh,const Visit& visit) {
    uint32_t previous=0;
    bool havePrevious=false;
    for(const auto& triangle:mesh.triangles) {
        if(havePrevious&&triangle.textureId==previous)continue;
        previous=triangle.textureId;
        havePrevious=true;
        if(!visit(triangle.textureId))return false;
    }
    return true;
}
