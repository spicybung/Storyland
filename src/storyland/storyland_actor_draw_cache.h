#pragma once
#include "src/leeds/leeds_model.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

// Bind geometry is immutable during Play Test. Poses, paint and health remain live.
class StorylandActorDrawCache {
public:
    struct Entry {
        const void* triangles=nullptr;
        size_t pointCount=0,triangleCount=0;
        std::array<float,6> bounds{};
        std::set<std::string> damagedParts;
        std::vector<size_t> materialOrder;
        std::vector<uint8_t> alphas;
        bool orderReady=false;
        bool containsVehicleWheels=false;
    };
    void clear() {entries.clear();}
    const Entry& get(const StorylandModelFile& model,const std::vector<uint32_t>& colors) {
        const auto& points=model.previewPoints();const auto& triangles=model.previewTriangles();
        auto& entry=entries[points.data()];
        if(entry.pointCount!=points.size()||entry.triangleCount!=triangles.size()||entry.triangles!=triangles.data()) {
            entry={};entry.pointCount=points.size();entry.triangleCount=triangles.size();entry.triangles=triangles.data();
            if(!points.empty()) {
                entry.bounds={points[0].x,points[0].y,points[0].z,points[0].x,points[0].y,points[0].z};
                for(const auto& point:points) {
                    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))continue;
                    entry.bounds[0]=std::min(entry.bounds[0],point.x);entry.bounds[3]=std::max(entry.bounds[3],point.x);
                    entry.bounds[1]=std::min(entry.bounds[1],point.y);entry.bounds[4]=std::max(entry.bounds[4],point.y);
                    entry.bounds[2]=std::min(entry.bounds[2],point.z);entry.bounds[5]=std::max(entry.bounds[5],point.z);
                }
            }
            for(const auto& triangle:triangles) {
                if(triangle.damagedPart)entry.damagedParts.insert(triangle.vehiclePart);
                if(triangle.vehicleWheel)entry.containsVehicleWheels=true;
            }
        }
        bool changed=!entry.orderReady||entry.alphas.size()!=colors.size();
        if(!changed)for(size_t index=0;index<colors.size();++index)
            if(entry.alphas[index]!=uint8_t(colors[index]>>24)) {changed=true;break;}
        if(changed) {
            entry.alphas.resize(colors.size());
            for(size_t index=0;index<colors.size();++index)entry.alphas[index]=uint8_t(colors[index]>>24);
            entry.materialOrder.clear();entry.materialOrder.reserve(triangles.size());
            for(bool transparent:{false,true})for(size_t index=0;index<triangles.size();++index) {
                const auto material=triangles[index].materialIndex;
                const uint8_t alpha=material<entry.alphas.size()?entry.alphas[material]:255;
                if((alpha<255)==transparent)entry.materialOrder.push_back(index);
            }
            entry.orderReady=true;
        }
        return entry;
    }
private:
    std::map<const void*,Entry> entries;
};
