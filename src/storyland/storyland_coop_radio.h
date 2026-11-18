#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

struct StorylandCoopRadioSelection {
    bool available = false;
    size_t index = 0;
    double positionSeconds = 0;
};

// Used only on entry. Subsequent local station changes are never overwritten.
template <typename Station>
StorylandCoopRadioSelection storylandCoopRadioSelection(const std::string& key,
    uint32_t position, const std::vector<Station>& stations) {
    StorylandCoopRadioSelection result;
    if(key == "off") { result.available = true; result.index = stations.size(); return result; }
    if(key.empty() || key == "usertracks") return result;
    const auto station=std::find_if(stations.begin(),stations.end(),[&](const Station& item) { return item.key==key; });
    if(station==stations.end()) return result;
    result.available=true;result.index=size_t(station-stations.begin());
    if(position!=UINT32_MAX && position<=86400000u) result.positionSeconds=double(position)*.001;
    return result;
}
