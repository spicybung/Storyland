#pragma once

#include "src/leeds/leeds_archive.h"
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <vector>

inline StorylandNativePoint storylandIntroSceneAnchor(
    const StorylandNativeZones& zones,const StorylandNativePoint& missionPoint,
    bool introProtected) {
    if(!introProtected || (std::isfinite(missionPoint.x)&&std::isfinite(missionPoint.y)&&
       std::hypot(missionPoint.x,missionPoint.y)>1.0f))return missionPoint;
    // The MAIN thread initially leaves Victor at the null position while
    // its first cutscene establishes the truck and base. Streaming must not
    // use (0,0) as the centre of that opening shot. This zone is authored
    // by the retail GAME.DTZ and supplies actual base bounds.
    for(const auto& zone:zones.information)if(zone.name=="ARMYBAS") {
        return {(zone.minimum.x+zone.maximum.x)*0.5f,
                (zone.minimum.y+zone.maximum.y)*0.5f,
                (zone.minimum.z+zone.maximum.z)*0.5f};
    }
    return missionPoint;
}

// Each native camera sector already supplies the complete authored scene.
template<class Browser>
inline std::vector<bool> storylandIntroVisibleSectors(
    const Browser& archive,
    const StorylandNativePoint& camera,
    const StorylandNativePoint& missionFocus,
    bool introProtected, bool cinematic) {
    (void)cinematic;
    auto result=archive.visibleSectorsAt(camera.x,camera.y,true,0);
    if(!introProtected||std::any_of(result.begin(),result.end(),[](bool visible){return visible;}))return result;
    if(!std::isfinite(missionFocus.x)||!std::isfinite(missionFocus.y))return result;
    return archive.visibleSectorsAt(missionFocus.x,missionFocus.y,true,0);
}

inline uint32_t storylandIntroWorldLevel(const StorylandNativeZones& zones,
    const StorylandNativePoint& camera,const StorylandNativePoint& missionFocus,
    bool introProtected,uint32_t previous) {
    const StorylandNativePoint& location=introProtected&&
        std::isfinite(missionFocus.x)&&std::isfinite(missionFocus.y)?missionFocus:camera;
    return zones.worldLevelAt(location,previous);
}
