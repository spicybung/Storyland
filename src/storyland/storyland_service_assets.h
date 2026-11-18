#pragma once
#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_service_roles.h"
#include <algorithm>

inline bool storylandLoadServicePedestrians(const StorylandDtzArchive& archive,
    std::vector<StorylandPlayTestAsset>& assets,std::string* error=nullptr) {
    static const char* names[]{"cop","swat","fbi","army","medic","fireman",
        "vice1","vice2","dea1","dea2","bcop","Gang4A","Gang4B"};
    for(const char* name:names) {
        uint32_t model=0;
        if(!archive.findModelIdByName(name,model))continue;
        if(std::any_of(assets.begin(),assets.end(),[&](const auto& asset) {
            return asset.pedStats.valid&&asset.pedStats.modelId==model;
        }))continue;
        std::vector<StorylandPlayTestAsset> loaded;std::string diagnostic;
        if(!storylandLoadPlayTestModels(archive,{name},loaded,diagnostic)) {
            if(diagnostic.find("PS2 DMA/VIF/VU/FIFO [")!=std::string::npos) {
                if(error)*error=diagnostic;
                return false;
            }
            continue;
        }
        for(auto& asset:loaded)
            if(asset.pedStats.valid&&asset.pedStats.modelId==model)
                assets.push_back(std::move(asset));
    }
    return true;
}
