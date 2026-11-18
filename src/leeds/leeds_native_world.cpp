// Native layouts cross-checked with https://github.com/aap/librwgta
// tools/storiesconv/leedsgta.h (eb06ee545af46298696532b6f394cf83ac1337c3)
// and tools/storiesview/collision.cpp. BLeeds leedsLib/col2.py also documents
// the PS2 COL2 records. Coordinates and surfaces come from the native data.
#include "src/leeds/leeds_native_world.h"
#include "src/leeds/leeds_native_layout.h"
#include "src/storyland/storyland_game_files.h"
#include "src/storyland/storyland_vehicle_route.h"
#include "src/storyland/storyland_play_test_camera.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <filesystem>
#include <cwctype>
#include <sstream>

void storylandPrepareNativeCollisionIndex(StorylandNativeWorld& world) {
    world.collisionCells.clear();world.collisionLargeInstances.clear();
    world.collisionIndexOwner=nullptr;
    for(size_t index=0;index<world.instances.size();++index) {
        const auto& instance=world.instances[index];
        const auto found=world.collisionModels.find(instance.modelId);
        if(found==world.collisionModels.end())continue;
        const auto& model=found->second;const auto& m=instance.matrix;
        if(!world.collisionModelEnabled(model))continue;
        const StorylandNativePoint center{(model.minimum.x+model.maximum.x)*0.5f,
            (model.minimum.y+model.maximum.y)*0.5f,(model.minimum.z+model.maximum.z)*0.5f};
        const StorylandNativePoint extent{(model.maximum.x-model.minimum.x)*0.5f,
            (model.maximum.y-model.minimum.y)*0.5f,(model.maximum.z-model.minimum.z)*0.5f};
        const float x=m[0]*center.x+m[4]*center.y+m[8]*center.z+m[12];
        const float y=m[1]*center.x+m[5]*center.y+m[9]*center.z+m[13];
        const float ex=std::fabs(m[0])*extent.x+std::fabs(m[4])*extent.y+std::fabs(m[8])*extent.z+0.15f;
        const float ey=std::fabs(m[1])*extent.x+std::fabs(m[5])*extent.y+std::fabs(m[9])*extent.z+0.15f;
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(ex)||!std::isfinite(ey)||
            ex<0||ey<0||std::fabs(x)+ex>1e8f||std::fabs(y)+ey>1e8f) {
            world.collisionLargeInstances.push_back(uint32_t(index));continue;
        }
        const int32_t x0=int32_t(std::floor((x-ex)/StorylandCollisionCellSize));
        const int32_t x1=int32_t(std::floor((x+ex)/StorylandCollisionCellSize));
        const int32_t y0=int32_t(std::floor((y-ey)/StorylandCollisionCellSize));
        const int32_t y1=int32_t(std::floor((y+ey)/StorylandCollisionCellSize));
        if(int64_t(x1-x0+1)*int64_t(y1-y0+1)>4096) {
            world.collisionLargeInstances.push_back(uint32_t(index));continue;
        }
        for(int32_t cellY=y0;cellY<=y1;++cellY)
            for(int32_t cellX=x0;cellX<=x1;++cellX)
                world.collisionCells[{cellX,cellY}].push_back(uint32_t(index));
    }
    world.collisionIndexOwner=world.instances.data();
    world.collisionIndexCount=world.instances.size();
    world.collisionIndexModels=world.collisionModels.size();
}

bool storylandSetNativeInstanceMatrix(StorylandNativeWorld& world,size_t index,
    const std::array<float,16>& matrix) {
    if(index>=world.instances.size()||
        !std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);}))return false;
    auto& instance=world.instances[index];
    if(instance.matrix==matrix)return true;
    if(world.collisionIndexOwner!=world.instances.data()||
        world.collisionIndexCount!=world.instances.size()||world.collisionIndexModels!=world.collisionModels.size()) {
        instance.matrix=matrix;storylandPrepareNativeCollisionIndex(world);return true;
    }
    const auto found=world.collisionModels.find(instance.modelId);
    if(found==world.collisionModels.end()) {instance.matrix=matrix;return true;}
    const auto& model=found->second;
    const StorylandNativePoint center{(model.minimum.x+model.maximum.x)*.5f,
        (model.minimum.y+model.maximum.y)*.5f,(model.minimum.z+model.maximum.z)*.5f};
    const StorylandNativePoint extent{(model.maximum.x-model.minimum.x)*.5f,
        (model.maximum.y-model.minimum.y)*.5f,(model.maximum.z-model.minimum.z)*.5f};
    const auto update=[&](const std::array<float,16>& transform,bool add) {
        const float x=transform[0]*center.x+transform[4]*center.y+transform[8]*center.z+transform[12];
        const float y=transform[1]*center.x+transform[5]*center.y+transform[9]*center.z+transform[13];
        const float ex=std::fabs(transform[0])*extent.x+std::fabs(transform[4])*extent.y+std::fabs(transform[8])*extent.z+.15f;
        const float ey=std::fabs(transform[1])*extent.x+std::fabs(transform[5])*extent.y+std::fabs(transform[9])*extent.z+.15f;
        const auto change=[&](std::vector<uint32_t>& entries) {
            const auto entry=std::lower_bound(entries.begin(),entries.end(),uint32_t(index));
            if(add) {
                if(entry==entries.end()||*entry!=uint32_t(index))entries.insert(entry,uint32_t(index));
            } else if(entry!=entries.end()&&*entry==uint32_t(index))entries.erase(entry);
        };
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(ex)||!std::isfinite(ey)||
            ex<0||ey<0||std::fabs(x)+ex>1e8f||std::fabs(y)+ey>1e8f) {
            change(world.collisionLargeInstances);return;
        }
        const int32_t x0=int32_t(std::floor((x-ex)/StorylandCollisionCellSize));
        const int32_t x1=int32_t(std::floor((x+ex)/StorylandCollisionCellSize));
        const int32_t y0=int32_t(std::floor((y-ey)/StorylandCollisionCellSize));
        const int32_t y1=int32_t(std::floor((y+ey)/StorylandCollisionCellSize));
        if(int64_t(x1-x0+1)*int64_t(y1-y0+1)>4096) {change(world.collisionLargeInstances);return;}
        for(int32_t cellY=y0;cellY<=y1;++cellY)for(int32_t cellX=x0;cellX<=x1;++cellX) {
            const auto key=std::make_pair(cellX,cellY);
            if(add)change(world.collisionCells[key]);
            else {
                const auto cell=world.collisionCells.find(key);
                if(cell==world.collisionCells.end())continue;
                change(cell->second);if(cell->second.empty())world.collisionCells.erase(cell);
            }
        }
    };
    update(instance.matrix,false);instance.matrix=matrix;
    if(world.collisionModelEnabled(model))update(instance.matrix,true);
    return true;
}

size_t storylandApplyBuildingSwaps(StorylandNativeWorld& world,
    const std::vector<StorylandBuildingSwap>& swaps,const std::map<uint32_t,int32_t>& states) {
    std::map<uint32_t,uint32_t> replacements;
    for(const auto& swap:swaps) {
        const auto state=states.find(swap.hash);
        if(state==states.end())continue;
        if(state->second==0)replacements[swap.buildingIndex]=swap.originalModel;
        else if(state->second>=0&&uint32_t(state->second)==swap.state)
            replacements[swap.buildingIndex]=swap.replacementModel;
    }
    size_t changed=0;
    for(auto& instance:world.instances) {
        if(instance.poolHeader!=0x24u)continue;
        const auto replacement=replacements.find(instance.poolIndex);
        if(replacement==replacements.end())continue;
        const bool enableCollision=world.paths.vcs&&!instance.nativeUsesCollision;
        if(instance.modelId==replacement->second&&!enableCollision)continue;
        instance.modelId=replacement->second;
        // VCS 0x23D46C calls 0x32E660, which sets entity flag 0x200 even
        // when the selected model matches its previous model index.
        if(world.paths.vcs)instance.nativeUsesCollision=true;
        ++changed;
    }
    if(changed)storylandPrepareNativeCollisionIndex(world);
    return changed;
}

void storylandResetBuildingSwaps(StorylandNativeWorld& world) {
    bool changed=false;
    for(auto& instance:world.instances) {
        const auto original=world.originalScriptInstanceMatrices.find((uint64_t(instance.poolHeader)<<32)|instance.poolIndex);
        if(original!=world.originalScriptInstanceMatrices.end()) {instance.matrix=original->second;changed=true;}
    }
    world.originalScriptInstanceMatrices.clear();
    for(auto& instance:world.instances)if(instance.modelId!=instance.originalModelId) {
        instance.modelId=instance.originalModelId;
        changed=true;
    }
    if(changed)storylandPrepareNativeCollisionIndex(world);
}

bool storylandSetNativeInstanceTransform(StorylandNativeWorld& world,uint64_t worldKey,const std::array<float,16>& matrix) {
    for(float value:matrix)if(!std::isfinite(value))return false;
    if(matrix[3]!=0||matrix[7]!=0||matrix[11]!=0||matrix[15]!=1)return false;
    for(size_t index=0;index<world.instances.size();++index) {
        auto& instance=world.instances[index];
        if(((uint64_t(instance.poolHeader)<<32)|instance.poolIndex)!=worldKey)continue;
        if(instance.matrix!=matrix) {
            world.originalScriptInstanceMatrices.emplace(worldKey,instance.matrix);
            return storylandSetNativeInstanceMatrix(world,index,matrix);
        }
        return true;
    }
    return false;
}

bool storylandReplaceNativeBuildingModel(StorylandNativeWorld& world,uint64_t worldKey,uint32_t model) {
    for(auto& instance:world.instances) {
        if(((uint64_t(instance.poolHeader)<<32)|instance.poolIndex)!=worldKey)continue;
        if(instance.modelId!=model) {
            instance.modelId=model;
            storylandPrepareNativeCollisionIndex(world);
        }
        return true;
    }
    return false;
}

void storylandSetFortStauntonDestroyed(StorylandNativeWorld& world,bool destroyed) {
    if(world.fortStauntonDestroyed==destroyed)return;
    world.fortStauntonDestroyed=destroyed;
    storylandPrepareNativeCollisionIndex(world);
}

namespace {
StorylandNativePoint lcsCollisionLevelPosition(int32_t level) {
    // Native CColStore::LevelPos: collision bounds are tested at each island's
    // representative point, not at the individual requested coordinate.
    if(level==2)return {350.0f,-624.0f,0};
    if(level==3)return {-670.0f,-511.0f,0};
    return {1060.0f,-800.0f,0};
}
bool lcsCollisionBankWanted(const StorylandNativeWorld& world,const StorylandNativeCollisionBank& bank) {
    return bank.slot!=0&&(int32_t(bank.slot)!=world.fortStauntonCollisionSlot||!world.fortStauntonDestroyed)&&
        (int32_t(bank.slot)!=world.fortDestroyedCollisionSlot||world.fortStauntonDestroyed);
}
}

bool storylandRequestNativeCollision(StorylandNativeWorld& world,const StorylandNativePoint& position,
    int32_t fallbackLevel,std::string& error) {
    if(world.zones.vcs||world.collisionBanks.empty()) {
        error="LCS collision request requires native collision-bank metadata.";return false;
    }
    int32_t level=int32_t(world.zones.levelAt(position));
    if(level<1||level>3)level=fallbackLevel>=1&&fallbackLevel<=3?fallbackLevel:1;
    const auto point=lcsCollisionLevelPosition(level);
    std::set<uint8_t> requested;
    for(const auto& bank:world.collisionBanks) {
        if(!lcsCollisionBankWanted(world,bank)||!bank.contains(point.x,point.y,-115.0f))continue;
        if(!bank.resident) {
            error="Requested collision bank '"+bank.name+"' has not been decoded from the companion IMG.";
            return false;
        }
        requested.insert(bank.slot);
    }
    world.requestedCollisionSlots.insert(requested.begin(),requested.end());
    if(world.collisionStreamingActive) {
        const size_t previous=world.activeCollisionSlots.size();
        world.activeCollisionSlots.insert(requested.begin(),requested.end());
        if(world.activeCollisionSlots.size()!=previous)storylandPrepareNativeCollisionIndex(world);
    }
    return true;
}

void storylandSetNativeCollisionLevels(StorylandNativeWorld& world,int32_t level,
    const std::array<int32_t,2>& allowedLevels,bool enabled) {
    if(world.zones.vcs||world.collisionBanks.empty()||!enabled)return;
    if(level<1||level>3)level=1;
    const int32_t allowed=allowedLevels[0]!=0?allowedLevels[0]:allowedLevels[1];
    const auto currentPoint=lcsCollisionLevelPosition(level),allowedPoint=lcsCollisionLevelPosition(allowed);
    std::set<uint8_t> active;
    for(const auto& bank:world.collisionBanks) {
        if(!bank.resident||!lcsCollisionBankWanted(world,bank))continue;
        bool wanted;
        if(bank.name=="indust")wanted=allowed!=0&&level!=1?allowed==1:level==1;
        else wanted=bank.contains(currentPoint.x,currentPoint.y)||
            (allowed>=1&&allowed<=3&&bank.contains(allowedPoint.x,allowedPoint.y));
        if(wanted)active.insert(bank.slot);
    }
    active.insert(world.requestedCollisionSlots.begin(),world.requestedCollisionSlots.end());
    world.requestedCollisionSlots.clear();
    if(!world.collisionStreamingActive||active!=world.activeCollisionSlots) {
        world.collisionStreamingActive=true;world.activeCollisionSlots=std::move(active);
        storylandPrepareNativeCollisionIndex(world);
    }
}

void storylandResetNativeCollisionStreaming(StorylandNativeWorld& world) {
    const bool active=world.collisionStreamingActive;
    world.collisionStreamingActive=false;world.activeCollisionSlots.clear();world.requestedCollisionSlots.clear();
    if(active)storylandPrepareNativeCollisionIndex(world);
}

bool storylandLoadNativeSceneCollision(StorylandNativeWorld& world,const StorylandNativePoint& position,
    int32_t fallbackLevel,const std::array<int32_t,2>& allowedLevels,bool enabled,std::string& error) {
    if(!enabled)return true; // Native CColStore::LoadCollision honors disabled streaming.
    if(world.zones.vcs||world.collisionBanks.empty()) {
        error="LOAD_SCENE requires native LCS collision-bank metadata.";return false;
    }
    int32_t level=int32_t(world.zones.levelAt(position));
    if(level<1||level>3)level=fallbackLevel>=1&&fallbackLevel<=3?fallbackLevel:1;
    const int32_t allowed=allowedLevels[0]!=0?allowedLevels[0]:allowedLevels[1];
    const auto point=lcsCollisionLevelPosition(level),extra=lcsCollisionLevelPosition(allowed);
    for(const auto& bank:world.collisionBanks) {
        if(!lcsCollisionBankWanted(world,bank))continue;
        const bool wanted=bank.name=="indust"?(allowed!=0&&level!=1?allowed==1:level==1):
            bank.contains(point.x,point.y)||(allowed>=1&&allowed<=3&&bank.contains(extra.x,extra.y));
        if(wanted&&!bank.resident) {
            error="LOAD_SCENE collision bank '"+bank.name+"' has not been decoded from the companion IMG.";
            return false;
        }
    }
    storylandSetNativeCollisionLevels(world,level,allowedLevels,true);
    return true;
}

int32_t storylandBuildingSwapHandle(const StorylandNativeWorld& world,
    const std::vector<StorylandBuildingSwap>& swaps,uint32_t hash) {
    // Native 23D158 selects the first matching live building; 21B8D0 tags
    // its pool index with 0x01000000 for the script entity resolver.
    for(const auto& swap:swaps) {
        if(swap.hash!=hash)continue;
        const auto instance=std::find_if(world.instances.begin(),world.instances.end(),
            [&](const auto& value) {
                return value.poolHeader==0x24u&&value.poolIndex==swap.buildingIndex;
            });
        if(instance!=world.instances.end()&&instance->poolIndex<0x800000u)
            return int32_t(0x1000000u|instance->poolIndex);
    }
    return -1;
}

bool storylandBuildingSwapBounds(const StorylandNativeWorld& world,
    const std::vector<StorylandBuildingSwap>& swaps, uint32_t hash,
    std::array<float,3>& center, float& radius) {
    center = {};
    radius = 0;
    // Native 0x23D1F0 uses the first matching swap record's active building
    // and its current model COL. The radius remains in model units.
    for (const auto& swap : swaps) {
        if (swap.hash != hash) {
            continue;
        }
        const auto instance = std::find_if(world.instances.begin(), world.instances.end(),
            [&](const auto& value) {
                return value.poolHeader == 0x24u && value.poolIndex == swap.buildingIndex;
            });
        if (instance == world.instances.end()) {
            continue;
        }
        const auto model = world.collisionModels.find(instance->modelId);
        if (model == world.collisionModels.end()) {
            return false;
        }
        const auto& local = model->second.boundingCenter;
        const auto& matrix = instance->matrix;
        center = {
            matrix[0] * local.x + matrix[4] * local.y + matrix[8] * local.z + matrix[12],
            matrix[1] * local.x + matrix[5] * local.y + matrix[9] * local.z + matrix[13],
            matrix[2] * local.x + matrix[6] * local.y + matrix[10] * local.z + matrix[14]
        };
        radius = model->second.boundingRadius;
        return true;
    }
    return false;
}

namespace {
bool contains(const std::vector<uint8_t>& data, uint64_t offset, uint64_t count, uint64_t stride = 1) {
    return offset <= data.size() && count <= (data.size() - offset) / stride;
}
uint16_t read16(const std::vector<uint8_t>& data, size_t offset) {
    return uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
}
uint32_t read32(const std::vector<uint8_t>& data, size_t offset) {
    return uint32_t(read16(data, offset)) | (uint32_t(read16(data, offset + 2)) << 16);
}
float readFloat(const std::vector<uint8_t>& data, size_t offset) {
    const uint32_t raw = read32(data, offset);
    float value;
    std::memcpy(&value, &raw, 4);
    return value;
}
StorylandNativePoint readPoint(const std::vector<uint8_t>& data, size_t offset) {
    return {readFloat(data, offset), readFloat(data, offset + 4), readFloat(data, offset + 8)};
}
bool finitePoint(const StorylandNativePoint& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
        std::fabs(p.x) <= 1000000 && std::fabs(p.y) <= 1000000 && std::fabs(p.z) <= 1000000;
}
float pointDistance(const StorylandNativePoint& a, const StorylandNativePoint& b) {
    const float x = a.x-b.x, y = a.y-b.y, z = a.z-b.z;
    return std::sqrt(x*x+y*y+z*z);
}
uint32_t chooseNext(const StorylandNativePaths& paths, uint32_t node, uint32_t previous,
    bool vehicle, uint32_t routeSeed = 0, const StorylandNativeZones* zones = nullptr,
    bool avoidLevelTransitions = false, bool ignoreDisabledRoads = false) {
    if (node >= paths.nodes.size()) return UINT32_MAX;
    const auto& source = paths.nodes[node];
    uint32_t reverse = UINT32_MAX;
    uint32_t selected = UINT32_MAX;
    float bestScore = -std::numeric_limits<float>::infinity();
    const bool haveIncoming = previous < paths.nodes.size();
    const auto incoming = haveIncoming ? paths.nodes[previous].position : source.position;
    const float inX = source.position.x - incoming.x;
    const float inY = source.position.y - incoming.y;
    const float inLength = std::hypot(inX, inY);
    for (const auto& link : source.links) {
        if(link.target>=paths.nodes.size() || (vehicle && !source.water && link.nativeLanes && !link.lanes)) continue;
        const auto& target = paths.nodes[link.target];
        const bool disabledRoadAllowed = paths.vcs && vehicle && !source.water && ignoreDisabledRoads;
        if (target.water != source.water || target.vehicle != vehicle ||
            (target.disabled && !disabledRoadAllowed) ||
            pointDistance(source.position, target.position) < 0.001f) continue;
        if (paths.vcs && avoidLevelTransitions && zones) {
            const auto* fromZone = zones->at(source.position, false);
            const auto* toZone = zones->at(target.position, false);
            if (fromZone && toZone && fromZone->level != toZone->level) continue;
        }
        if (link.target == previous) { reverse = link.target; continue; }
        // Native random-route cars can leave a transition they already occupy,
        // but a stay-in-current-level car cannot enter one from an ordinary road.
        if(!paths.vcs&&vehicle&&avoidLevelTransitions&&(target.flags&0x40u)&&
            !(haveIncoming&&(paths.nodes[previous].flags&0x40u)))continue;
        const float outX = target.position.x - source.position.x;
        const float outY = target.position.y - source.position.y;
        const float outLength = std::hypot(outX, outY);
        float score = 0.0f;
        if (inLength > 0.001f && outLength > 0.001f)
            score = (inX * outX + inY * outY) / (inLength * outLength);
        // Pedestrians generally continue along the footpath instead of
        // crossing a road at every available junction.
        if (!vehicle && link.crossesRoad) score -= 0.18f;
        if (routeSeed != 0) {
            uint32_t variation = routeSeed ^ (link.target * 0x9E3779B9u) ^
                (node * 0x85EBCA6Bu);
            variation ^= variation >> 16;
            variation *= 0x7FEB352Du;
            variation ^= variation >> 15;
            score += (float(variation & 1023u) / 1023.0f - 0.5f) * 0.95f;
        }
        if (score > bestScore) { bestScore = score; selected = link.target; }
    }
    return selected != UINT32_MAX ? selected : reverse;
}

const StorylandNativePathLink* roadLink(const StorylandNativePaths& paths,uint32_t a,uint32_t b) {
    if(a>=paths.nodes.size())return nullptr;
    for(const auto& link:paths.nodes[a].links)if(link.target==b)return &link;
    return nullptr;
}
float roadOffset(const StorylandNativePaths& paths,uint32_t a,uint32_t b,unsigned lane,float fallback) {
    if(a<paths.nodes.size()&&paths.nodes[a].water)return 0;
    const auto* link=roadLink(paths,a,b);
    if(!link||!link->nativeLanes)return fallback;
    if(!link->lanes)return 0;
    lane=std::min(lane,unsigned(link->lanes-1));
    // SLUS_215.90 0x127270..0x127390: 5-unit lane pitch;
    // one-way roads are centered, two-way roads include native width / 16.
    if(!link->oppositeLanes)return (2.0f*lane+1-link->lanes)*2.5f;
    return (2.0f*lane+1)*2.5f+link->dividerWidth;
}
StorylandNativePoint lanePosition(const StorylandNativePaths& paths,
    uint32_t fromIndex,uint32_t toIndex,float ratio,float offset,float groundOffset,
    uint32_t previousIndex=UINT32_MAX,unsigned lane=0,uint32_t followingIndex=UINT32_MAX) {
    const auto& from=paths.nodes[fromIndex].position;
    const auto& to=paths.nodes[toIndex].position;
    const float dx=to.x-from.x,dy=to.y-from.y;
    const float length=std::hypot(dx,dy);
    const float nx=length>0.001f?dy/length:0,ny=length>0.001f?-dx/length:0;
    if(paths.nodes[fromIndex].vehicle)offset=roadOffset(paths,fromIndex,toIndex,lane,offset);
    auto joint=[&](uint32_t other,bool incoming) {
        if(other>=paths.nodes.size())return StorylandNativePoint{nx*offset,ny*offset,0};
        const auto& p=paths.nodes[other].position;
        const float x=incoming?from.x-p.x:p.x-to.x,y=incoming?from.y-p.y:p.y-to.y;
        const float l=std::hypot(x,y);
        if(l<0.001f)return StorylandNativePoint{nx*offset,ny*offset,0};
        const float otherOffset=incoming?roadOffset(paths,other,fromIndex,lane,offset):
            roadOffset(paths,toIndex,other,lane,offset);
        // A shared, bounded corner anchor keeps both adjacent edges continuous.
        return StorylandNativePoint{(nx*offset+y/l*otherOffset)*0.5f,
            (ny*offset-x/l*otherOffset)*0.5f,0};
    };
    const auto start=joint(previousIndex,true),end=joint(followingIndex,false);
    const float smooth=ratio*ratio*(3-2*ratio);
    float centerX=from.x+dx*ratio,centerY=from.y+dy*ratio;
    if(paths.nodes[fromIndex].vehicle&&!paths.nodes[fromIndex].water&&length>0.001f) {
        const auto roundCorner=[&](uint32_t other,bool incoming) {
            if(other>=paths.nodes.size())return;
            const auto& p=paths.nodes[other].position;
            const float ox=incoming?from.x-p.x:p.x-to.x,oy=incoming?from.y-p.y:p.y-to.y;
            const float otherLength=std::hypot(ox,oy);if(otherLength<.001f)return;
            const float cut=std::min({5.0f,length*.2f,otherLength*.2f});
            const float along=incoming?ratio*length:(1-ratio)*length;
            if(cut<.001f||along>=cut)return;
            const float t=incoming?.5f+.5f*along/cut:.5f-.5f*along/cut;
            const auto corner=incoming?from:to;
            const float ix=incoming?ox/otherLength:dx/length,iy=incoming?oy/otherLength:dy/length;
            const float ex=incoming?dx/length:ox/otherLength,ey=incoming?dy/length:oy/otherLength;
            centerX=corner.x-ix*cut*(1-t)*(1-t)+ex*cut*t*t;
            centerY=corner.y-iy*cut*(1-t)*(1-t)+ey*cut*t*t;
        };
        roundCorner(previousIndex,true);roundCorner(followingIndex,false);
    }
    return {centerX+start.x*(1-smooth)+end.x*smooth,
        centerY+start.y*(1-smooth)+end.y*smooth,
        from.z+(to.z-from.z)*ratio+groundOffset};
}
}

bool StorylandNativeZone::contains(const StorylandNativePoint& point) const {
    return point.x >= minimum.x && point.x <= maximum.x &&
        point.y >= minimum.y && point.y <= maximum.y &&
        point.z >= minimum.z && point.z <= maximum.z;
}

const StorylandNativeZone* StorylandNativeZones::named(const std::string& name) const {
    for (const auto& zone : navigation)
        if (zone.name == name) return &zone;
    return nullptr;
}

const StorylandNativeZone* StorylandNativeZones::at(const StorylandNativePoint& point,
    bool informationZone) const {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) return nullptr;
    const auto& table = informationZone ? information : navigation;
    if (table.empty()) return nullptr;
    const StorylandNativeZone* selected = &table.front();
    if(!vcs) {
        if(informationZone) {
            for(size_t index=1;index<table.size();++index)
                if(table[index].contains(point))return &table[index];
            return selected;
        }
        int32_t index=selected->child;
        for(size_t visited=0;visited<table.size()&&index>=0&&size_t(index)<table.size();++visited) {
            const auto& zone=table[size_t(index)];
            if(zone.contains(point)) {selected=&zone;index=zone.child;}
            else index=zone.next;
        }
        return selected;
    }
    // Native VCS 0x367AC8 / 0x367BB0 retain the last containing row.
    for (size_t index = 1; index < table.size(); ++index)
        if (table[index].contains(point)) selected = &table[index];
    return selected;
}

uint32_t StorylandNativeZones::levelAt(const StorylandNativePoint& point) const {
    if(!vcs&&!map.empty()) {
        for(size_t index=1;index<map.size();++index)
            if(map[index].contains(point))return map[index].level;
        return map.front().level;
    }
    const auto* zone=at(point,false);
    return zone?zone->level:0;
}

uint32_t StorylandNativeZones::worldLevelAt(const StorylandNativePoint& point,uint32_t currentLevel) const {
    if(!vcs)return levelAt(point);
    if(!finitePoint(point))return currentLevel;
    if(point.z<=-100.0f)return 3;
    uint32_t candidate=0;
    for(size_t index=1;index<map.size();++index) {
        const auto& zone=map[index];
        if(!zone.contains(point))continue;
        if(zone.level==currentLevel)return currentLevel;
        if(zone.level>=1&&zone.level<=3)candidate=zone.level;
    }
    return candidate?candidate:currentLevel;
}

bool storylandDecodeNativeZones(const std::vector<uint8_t>& data, bool vcs,
    StorylandNativeZones& zones, std::string& error) {
    zones = {};
    error.clear();
    const auto fail = [&](const char* message) {
        zones = {};
        zones.status = error = message;
        return false;
    };
    if (!contains(data, 0x48, 4)) return fail("DTZ zone pointer is truncated.");
    const uint32_t manager = read32(data, 0x48);
    if (manager < 0x20 || !contains(data, manager, vcs?0x70:0x6C))
        return fail("DTZ zone manager is outside the data block.");
    zones.vcs=vcs;
    // VCS stores its MAP pointer/count at +0x20/+0x6c; LCS uses +0x1c/+0x68.
    // The 56-byte CZone records retain child/parent/next pointers at +44/+48/+52.
    for (unsigned table = 0; table < 3u; ++table) {
        const uint32_t start = read32(data, manager + (table==2?(vcs?32:28):(vcs?12:8)+table*4));
        const uint16_t count = read16(data, manager + (table==2?(vcs?0x6C:0x68):(vcs?24:20)+table*2));
        if (!count || start < 0x20 || !contains(data, start, count, 56))
            return fail("DTZ zone table is missing or truncated.");
        auto& output = table==2?zones.map:table?zones.information:zones.navigation;
        output.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            const size_t offset = size_t(start) + size_t(index) * 56;
            StorylandNativeZone zone;
            for (size_t character = 0; character < 8 && data[offset + character]; ++character) {
                const uint8_t value = data[offset + character];
                if (value < 32 || value > 126) return fail("DTZ zone name is invalid.");
                zone.name.push_back(char(value));
            }
            zone.minimum = readPoint(data, offset + 8);
            zone.maximum = readPoint(data, offset + 20);
            zone.type = read32(data, offset + 32);
            zone.level = read32(data, offset + 36);
            zone.populationDay=read16(data,offset+40);
            zone.populationNight=read16(data,offset+42);
            if (vcs && table == 1) {
                const uint32_t records = read32(data, manager + 20);
                const uint16_t recordCount=read16(data,manager+28);
                zone.populationLoaded = true;
                for (unsigned period = 0; period < 2; ++period) {
                    const uint16_t infoIndex = read16(data, offset + (period ? 40 : 42));
                    const uint64_t info = uint64_t(records) + uint64_t(infoIndex) * 0x48;
                    if (records < 0x20 || infoIndex>=recordCount||
                        !contains(data,records,recordCount,0x48)||info+0x48>data.size()) {
                        zone.populationLoaded = false;
                        break;
                    }
                    auto& population=zone.populationRecords[period];
                    population.carDensity=read16(data,size_t(info));
                    for(size_t group=0;group<population.vehicleClassThresholds.size();++group)
                        population.vehicleClassThresholds[group]=read16(data,size_t(info)+2+group*2);
                    for(size_t gang=0;gang<population.gangCarThresholds.size();++gang) {
                        population.gangCarThresholds[gang]=read16(data,size_t(info)+0x1C+gang*2);
                        population.gangPedestrianThresholds[gang]=read16(data,size_t(info)+0x32+gang*2);
                    }
                    population.policeCarThreshold=read16(data,size_t(info)+0x2E);
                    population.pedestrianDensity=read16(data,size_t(info)+0x30);
                    population.policePedestrianThreshold=read16(data,size_t(info)+0x44);
                    population.pedestrianGroup=read16(data,size_t(info)+0x46);
                }
                if(!zone.populationLoaded)zone.populationRecords={};
            }
            if (zone.name.empty() || !finitePoint(zone.minimum) || !finitePoint(zone.maximum) ||
                zone.minimum.x > zone.maximum.x || zone.minimum.y > zone.maximum.y ||
                zone.minimum.z > zone.maximum.z || (table==2?zone.type!=3:table?zone.type!=2:zone.type>1))
                return fail("DTZ zone bounds or type are invalid.");
            if(!vcs&&table==0) {
                const auto link=[&](size_t field,int32_t& destination) {
                    const uint32_t pointer=read32(data,offset+field);
                    if(!pointer) {destination=-1;return true;}
                    if(pointer<start||(pointer-start)%56||uint64_t(pointer-start)>=uint64_t(count)*56)return false;
                    destination=int32_t((pointer-start)/56);return true;
                };
                if(!link(44,zone.child)||!link(52,zone.next))return fail("DTZ zone hierarchy has an invalid pointer.");
            }
            output.push_back(std::move(zone));
        }
    }
    // VCS moves the audio-zone index list four bytes after the LCS list.
    // Its native lookup at 0x368678 walks the stored indices in this order.
    const uint16_t audioCount=read16(data,manager+(vcs?0x6E:0x6A));
    if(audioCount>36) {
        if(!vcs)return fail("DTZ audio zone count exceeds its native table.");
    } else {
        zones.audio.reserve(audioCount);
        for(size_t index=0;index<audioCount;++index) {
            const uint16_t navigationIndex=read16(data,manager+(vcs?0x24:0x20)+index*2);
            if(navigationIndex>=zones.navigation.size()) {
                if(!vcs)return fail("DTZ audio zone references an invalid navigation zone.");
                // This layout is verified on PS2. An unknown VCS platform's
                // optional table must not prevent its world from loading.
                zones.audio.clear();
                break;
            }
            zones.audio.push_back(navigationIndex);
        }
    }
    if(!vcs) {
        const uint32_t populationStart=read32(data,manager+16);
        const uint16_t populationCount=read16(data,manager+24);
        constexpr size_t populationStride=68;
        if(!populationCount||populationCount>1024||populationStart<0x20||
            !contains(data,populationStart,populationCount,populationStride))
            return fail("DTZ zone population table is missing or truncated.");
        zones.populations.resize(populationCount);
        for(size_t index=0;index<populationCount;++index) {
            const size_t offset=size_t(populationStart)+index*populationStride;
            auto& population=zones.populations[index];
            population.cars[0]=int16_t(read16(data,offset));
            population.cars[10]=int16_t(read16(data,offset+42));
            population.pedestrians[0]=read16(data,offset+44);
            population.pedestrians[10]=read16(data,offset+64);
            population.pedestrianGroup=read16(data,offset+66);
            int32_t previousCar=population.cars[10],previousPed=population.pedestrians[10];
            int32_t previousCivilian=0;
            for(size_t gang=0;gang<9;++gang) {
                const int32_t car=int16_t(read16(data,offset+24+gang*2));
                const int32_t ped=read16(data,offset+46+gang*2);
                population.cars[gang+1]=car-previousCar;
                population.pedestrians[gang+1]=ped-previousPed;
                previousCar=car;previousPed=ped;
                const int32_t civilian=int16_t(read16(data,offset+2+gang*2));
                population.civilianCars[gang]=civilian-previousCivilian;previousCivilian=civilian;
            }
            population.boats[0]=int16_t(read16(data,offset+20));
            population.boats[1]=int16_t(read16(data,offset+22))-population.boats[0];
        }
        for(const auto& zone:zones.information)
            if(zone.populationDay>=populationCount||zone.populationNight>=populationCount)
                return fail("DTZ zone population index is outside its table.");
    }
    zones.status = std::to_string(zones.navigation.size()) + " navigation zones, " +
        std::to_string(zones.information.size()) + " information zones";
    zones.status+=", "+std::to_string(zones.map.size())+" map zones";
    return true;
}

bool storylandDecodeNativePaths(const std::vector<uint8_t>& data, bool vcs,
    StorylandNativePaths& paths, std::string& error) {
    paths = {};
    error.clear();
    auto fail = [&](const char* message) { paths = {}; paths.status = error = message; return false; };
    if (!contains(data, 0, 0x24)) return fail("DTZ path header is truncated.");
    const uint32_t base = read32(data, 0x20);
    StorylandVcsPspStreamingLayout streaming;
    const bool psp = vcs && storylandVcsPspStreamingLayout(data, streaming);
    const uint32_t vcsConnections = psp ? 0x7558u : 0x7BA0u;
    const uint32_t vcsGrid = psp ? 0x28u : 0x670u;
    if (base < 0x20 || !contains(data, base, vcs ? vcsConnections + 8u : 0x30)) return fail("DTZ has no complete native path object.");
    const uint32_t nodes = read32(data, base);
    const uint32_t count = read32(data, base + (vcs ? 0x0C : 0x14));
    const uint32_t cars = read32(data, base + (vcs ? 0x10 : 0x18));
    const uint32_t peds = read32(data, base + (vcs ? 0x14 : 0x1C));
    const uint32_t connections = read32(data, base + (vcs ? vcsConnections : 8));
    const uint32_t distances = read32(data, base + (vcs ? vcsConnections + 4u : 12));
    const uint32_t connectionCount = read16(data, base + (vcs ? 0x1A : 0x22));
    const size_t stride = vcs ? 10 : 20;
    if (count == 0 || count > 16384 || uint64_t(cars) + peds != count ||
        nodes == 0 || connections == 0 || distances == 0 ||
        !contains(data, nodes, count, stride) || !contains(data, connections, connectionCount, 2) ||
        !contains(data, distances, connectionCount)) return fail("Native path counts or buffers are invalid.");
    StorylandNativePaths decoded;
    decoded.vcs=vcs;
    decoded.vehicleNodeCount = cars;
    decoded.pedestrianNodeCount = peds;
    decoded.connectionCount = connectionCount;
    decoded.nodes.resize(count);
    const uint32_t carLinks=read32(data,base+4);
    const uint32_t carConnections=read32(data,base+(vcs?8:0x10));
    const uint32_t carLinkCount=read32(data,base+(vcs?0x20:0x28));
    const uint32_t carConnectionCount=read16(data,base+(vcs?0x1C:0x24));
    const size_t carLinkStride=vcs?4:12;
    if(carLinkCount>16384||carConnectionCount>connectionCount||
        !contains(data,carLinks,carLinkCount,carLinkStride)||
        !contains(data,carConnections,carConnectionCount,2))
        return fail("Native car lane buffers are truncated.");
    uint64_t linksRead = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const size_t offset = size_t(nodes) + index * stride;
        auto& node = decoded.nodes[index];
        const size_t xyz = offset + (vcs ? 0 : 4);
        node.position.x = int16_t(read16(data, xyz)) / 8.0f;
        node.position.y = int16_t(read16(data, xyz + 2)) / 8.0f;
        node.position.z = vcs ? float(int8_t(data[xyz + 4])) : int16_t(read16(data, xyz + 4)) / 8.0f;
        // Native VCS GetPosition (0x266C04..0x266C38) maps negative
        // compressed heights another 100 units below their signed value.
        if(vcs&&node.position.z<0)node.position.z-=100;
        node.vehicle = index < cars;
        const uint32_t first = read16(data, offset + (vcs ? 6 : 12));
        const uint8_t links = data[offset + (vcs ? 8 : 16)] & 15;
        node.flags = read16(data, offset + (vcs ? 8 : 16));
        if(vcs) {
            node.width=data[offset+5];
            node.water=node.vehicle&&(node.flags&0x100)!=0;
            node.disabled=(node.flags&0x20)!=0;
            node.speedLimit=(node.flags>>10)&3;
        }
        if (!vcs) {
            node.width = data[offset + 14];
            node.water = node.vehicle && (data[offset + 17] & 1) != 0;
            node.disabled = (data[offset + 16] & 0x20) != 0;
            node.speedLimit = (data[offset + 17] >> 3) & 3;
            node.spawnRate = data[offset + 18] & 15;
        }
        if (uint64_t(first) + links > connectionCount) return fail("Native path link range exceeds the connection table.");
        for (uint32_t link = first; link < first + links; ++link) {
            const uint16_t raw = read16(data, size_t(connections) + link * 2);
            const uint32_t target = raw & 0x3FFF;
            if (target >= count) return fail("Native path connection points outside the node array.");
            StorylandNativePathLink decodedLink{};
            decodedLink.target=target;
            decodedLink.distance=data[size_t(distances)+link];
            decodedLink.trafficLight=(raw&0x4000)!=0;
            decodedLink.crossesRoad=(raw&0x8000)!=0;
            if(node.vehicle) {
                if(link>=carConnectionCount)return fail("Native car connection range is invalid.");
                const uint16_t carIndex=read16(data,size_t(carConnections)+link*2);
                if(carIndex>=carLinkCount)return fail("Native car link index is invalid.");
                const size_t record=size_t(carLinks)+carIndex*carLinkStride;
                const uint32_t owner=vcs?read16(data,record)&0x3FFF:read16(data,record+4);
                if(owner!=index&&owner!=target)return fail("Native car link endpoint is inconsistent.");
                if(vcs&&owner!=std::min(index,target))return fail("VCS car link owner is inconsistent.");
                const uint8_t flags=data[record+(vcs?2:8)];
                decodedLink.nativeLanes=true;decodedLink.carLink=carIndex;
                decodedLink.signalGroup=vcs?uint8_t(read16(data,record)>>14):data[record+9]&3;
                decodedLink.signalApplies=decodedLink.signalGroup!=0&&((owner==target)!=((flags&0x40)!=0));
                const bool left=vcs?index<target:owner==target;
                decodedLink.lanes=left?flags&7:(flags>>3)&7;
                decodedLink.oppositeLanes=left?(flags>>3)&7:flags&7;
                if(vcs) {
                    const size_t targetOffset=size_t(nodes)+target*stride;
                    decodedLink.dividerWidth=std::min(node.width,data[targetOffset+5])/16.0f;
                } else {
                    decodedLink.dividerWidth=data[record+10]/16.0f;
                    decodedLink.nativeStopLine=true;
                    decodedLink.stopPosition={int16_t(read16(data,record))/8.0f,
                        int16_t(read16(data,record+2))/8.0f,node.position.z};
                    decodedLink.stopDirection={int8_t(data[record+6])/100.0f,
                        int8_t(data[record+7])/100.0f,0};
                    if(owner==index) {
                        decodedLink.stopDirection.x=-decodedLink.stopDirection.x;
                        decodedLink.stopDirection.y=-decodedLink.stopDirection.y;
                    }
                }
            }
            node.links.push_back(decodedLink);
        }
        linksRead += links;
    }
    if (linksRead != connectionCount) return fail("Native path connection count does not match the node link ranges.");
    if(vcs) {
        decoded.vcsSearchGrid.resize(2500);
        for(size_t cell=0;cell<decoded.vcsSearchGrid.size();++cell)for(size_t kind=0;kind<2;++kind) {
            const size_t entry=size_t(base)+vcsGrid+cell*12;
            const uint32_t list=read32(data,entry+kind*4);
            const uint8_t length=data[entry+8+kind];
            if(length&&(!list||!contains(data,list,length,2)))
                return fail("VCS path search grid list is truncated.");
            auto& indices=decoded.vcsSearchGrid[cell][kind];indices.reserve(length);
            for(size_t index=0;index<length;++index) {
                const uint16_t node=read16(data,size_t(list)+index*2);
                if(node>=count||(node<cars)!=(kind==0))
                    return fail("VCS path search grid has an invalid node or type.");
                indices.push_back(node);
            }
        }
    } else if(contains(data,size_t(base)+0x2E80,10000,12)) {
        decoded.lcsSearchGrid.resize(10000);
        for(size_t cell=0;cell<decoded.lcsSearchGrid.size();++cell)for(size_t kind=0;kind<2;++kind) {
            const size_t entry=size_t(base)+0x2E80+cell*12;
            const uint32_t list=read32(data,entry+kind*4);
            const uint16_t length=read16(data,entry+8+kind*2);
            if(length&&(!list||!contains(data,list,length,2)))
                return fail("LCS path search grid list is truncated.");
            auto& indices=decoded.lcsSearchGrid[cell][kind];indices.reserve(length);
            for(size_t index=0;index<length;++index) {
                const uint16_t node=read16(data,size_t(list)+index*2);
                if(node>=count||(node<cars)!=(kind==0))
                    return fail("LCS path search grid has an invalid node or type.");
                indices.push_back(node);
            }
        }
    }
    std::ostringstream status;
    status << (vcs ? "VCS" : "LCS") << ": " << cars << " vehicle nodes, " << peds
           << " pedestrian nodes, " << connectionCount << " links";
    decoded.status = status.str();
    paths = std::move(decoded);
    return true;
}

bool storylandDecodeNativeCollisionModel(const std::vector<uint8_t>& data, uint32_t offset,
    uint32_t limit, bool vcs, StorylandNativeCollisionModel& model, std::string& error) {
    model = {};
    error.clear();
    auto fail = [&](const char* message) { model = {}; error = message; return false; };
    // VCS PSP ends the collision header after its six pointers at +0x50.
    // The extra PS2 structure padding is not part of the data required here.
    const uint32_t headerBytes = vcs ? 0x50u : 0x60u;
    if (limit > data.size() || offset < 0x20 || uint64_t(offset) + headerBytes > limit)
        return fail("Native CColModel header is outside the data block.");
    const uint32_t sphereCount = read16(data, offset + (vcs ? 0x34 : 0x30));
    const uint32_t boxCount = read16(data, offset + (vcs ? 0x30 : 0x32));
    const uint32_t triangleCount = read16(data, offset + (vcs ? 0x32 : 0x34));
    if (sphereCount > 20000 || boxCount > 20000 || triangleCount > 20000)
        return fail("Native collision primitive counts are invalid.");
    const uint32_t pointerBase = offset + (vcs ? 0x38 : 0x3C);
    const uint32_t spheres = read32(data, pointerBase);
    const uint32_t boxes = read32(data, pointerBase + 8);
    const uint32_t vertices = read32(data, pointerBase + 16);
    const uint32_t triangles = read32(data, pointerBase + 20);
    const auto range = [limit](uint32_t start, uint32_t count, uint32_t stride) {
        return count == 0 || (start >= 0x20 && start <= limit && count <= (limit - start) / stride);
    };
    if (!range(spheres, sphereCount, 32) || !range(boxes, boxCount, 48) || !range(triangles, triangleCount, 8))
        return fail("Native collision primitive buffer is truncated.");
    StorylandNativeCollisionModel decoded;
    if(!vcs)decoded.collisionSlot=data[size_t(offset)+0x38];
    decoded.boundingCenter = readPoint(data, offset);
    decoded.boundingRadius = readFloat(data, offset + 0x0C);
    if (!finitePoint(decoded.boundingCenter) || !std::isfinite(decoded.boundingRadius) ||
        decoded.boundingRadius < 0 || decoded.boundingRadius > 100000)
        return fail("Native collision bounding sphere is invalid.");
    decoded.minimum = readPoint(data, offset + 0x10);
    decoded.maximum = readPoint(data, offset + 0x20);
    if (!finitePoint(decoded.minimum) || !finitePoint(decoded.maximum) ||
        decoded.minimum.x > decoded.maximum.x || decoded.minimum.y > decoded.maximum.y || decoded.minimum.z > decoded.maximum.z)
        return fail("Native collision bounds are invalid.");
    uint32_t vertexCount = 0;
    for (uint32_t index = 0; index < triangleCount; ++index) {
        const size_t row = size_t(triangles) + index * 8;
        StorylandNativeCollisionTriangle triangle;
        for (size_t corner = 0; corner < 3; ++corner) {
            const int16_t byteOffset = int16_t(read16(data, row + corner * 2));
            if (byteOffset < 0 || byteOffset % 6 != 0) return fail("Native collision vertex byte offset is invalid.");
            triangle.vertices[corner] = uint32_t(byteOffset) / 6;
            vertexCount = std::max(vertexCount, triangle.vertices[corner] + 1);
        }
        triangle.surface = data[row + 6];
        decoded.triangles.push_back(triangle);
    }
    if (!range(vertices, vertexCount, 6)) return fail("Native collision vertex buffer is truncated.");
    for (uint32_t index = 0; index < vertexCount; ++index) {
        const size_t row = size_t(vertices) + index * 6;
        decoded.vertices.push_back({int16_t(read16(data, row)) / 128.0f,
            int16_t(read16(data, row + 2)) / 128.0f, int16_t(read16(data, row + 4)) / 128.0f});
    }
    for (uint32_t index = 0; index < boxCount; ++index) {
        const size_t row = size_t(boxes) + index * 48;
        StorylandNativeCollisionBox box{readPoint(data, row), readPoint(data, row + 16), data[row + 32], data[row + 33]};
        if (!finitePoint(box.minimum) || !finitePoint(box.maximum) || box.minimum.x > box.maximum.x ||
            box.minimum.y > box.maximum.y || box.minimum.z > box.maximum.z) return fail("Native collision box is invalid.");
        decoded.boxes.push_back(box);
    }
    for (uint32_t index = 0; index < sphereCount; ++index) {
        const size_t row = size_t(spheres) + index * 32;
        StorylandNativeCollisionSphere sphere{readPoint(data, row), readFloat(data, row + 12), data[row + 16], data[row + 17]};
        if (!finitePoint(sphere.center) || !std::isfinite(sphere.radius) || sphere.radius < 0 || sphere.radius > 100000)
            return fail("Native collision sphere is invalid.");
        decoded.spheres.push_back(sphere);
    }
    model = std::move(decoded);
    return true;
}

bool storylandDecodeNativeCol2(const std::vector<uint8_t>& data, bool vcs,
    std::map<uint32_t, StorylandNativeCollisionModel>& models, std::string& error) {
    error.clear();
    if (!contains(data, 0, 32) || std::memcmp(data.data(), "2loc", 4) != 0) { error = "Not a native Leeds COL2 chunk."; return false; }
    const uint32_t size = read32(data, 8), limit = read32(data, 12);
    if (size > data.size() || size < 32 || limit < 32 || limit > size) { error = "Native COL2 chunk size is invalid."; return false; }
    std::map<uint32_t, StorylandNativeCollisionModel> decoded;
    uint32_t tableEnd = limit;
    bool terminated = false;
    for (uint32_t row = 32; uint64_t(row) + 8 <= tableEnd; row += 8) {
        const uint32_t id = read32(data, row), offset = read32(data, row + 4);
        if (id == UINT32_MAX || (id == 0 && offset == 0)) { terminated = true; break; }
        if (id > 100000 || offset < row + 8 || offset >= limit) { error = "Native COL2 resource table is invalid."; return false; }
        tableEnd = std::min(tableEnd, offset);
        StorylandNativeCollisionModel model;
        if (!storylandDecodeNativeCollisionModel(data, offset, limit, vcs, model, error)) return false;
        decoded[id] = std::move(model);
    }
    if (!terminated || decoded.empty()) { error = "Native COL2 resource table has no complete models or terminator."; return false; }
    for (auto& item : decoded) models[item.first] = std::move(item.second);
    return true;
}

void storylandDecodeNativeInstances(const std::vector<uint8_t>& data, bool vcs,
    std::vector<StorylandNativeCollisionInstance>& instances) {
    instances.clear();
    if (!contains(data, 0, 0x40)) return;
    const uint32_t modelCount = read32(data, 0x38);
    const uint32_t sectors=contains(data,0x4C,4)?read32(data,0x4C):0;
    const bool nativeSectorLists=vcs&&sectors>=0x20&&contains(data,sectors,2500,0x38);
    std::map<uint32_t,uint16_t> membership;
    if(nativeSectorLists) {
        // VCS 472698 indexes 50*50 sectors of 0x38 bytes. 1925C8 follows
        // CPtrNode entity/next at +0/+8; lists are separate query categories.
        std::array<std::set<uint32_t>,14> visited;
        for(uint32_t cell=0;cell<2500;++cell)for(uint32_t list=0;list<14;++list) {
            uint32_t node=read32(data,size_t(sectors)+cell*0x38+list*4);
            while(node>=0x20&&contains(data,node,12)&&visited[list].insert(node).second) {
                membership[read32(data,node)]|=uint16_t(1u<<list);
                node=read32(data,size_t(node)+8);
            }
        }
    }
    for (uint32_t field : {0x24u, 0x28u, 0x2Cu}) {
        const uint32_t pool = read32(data, field);
        if (pool == 0 || !contains(data, pool, 16)) continue;
        const uint32_t items = read32(data, pool), flags = read32(data, pool + 4), count = read32(data, pool + 8);
        if (count > 200000 || items == 0 || flags == 0 || !contains(data, items, count, 96) || !contains(data, flags, count)) continue;
        for (uint32_t index = 0; index < count; ++index) {
            if (data[size_t(flags) + index] & 128) continue;
            const size_t row = size_t(items) + index * 96;
            StorylandNativeCollisionInstance instance;
            instance.modelId = read16(data, row + (vcs ? 0x56 : 0x58));
            instance.poolHeader=field;
            instance.poolIndex=index;
            instance.originalModelId=instance.modelId;
            if(nativeSectorLists)instance.nativeSectorLists=membership[uint32_t(row)];
            if(vcs)instance.nativeUsesCollision=(data[row+0x49]&2)!=0;
            if (instance.modelId >= modelCount) continue;
            bool valid = true;
            for (size_t column = 0; column < 4; ++column) {
                for (size_t component = 0; component < 3; ++component) {
                    const float value = readFloat(data, row + column * 16 + component * 4);
                    if (!std::isfinite(value) || std::fabs(value) > 1000000) valid = false;
                    instance.matrix[column * 4 + component] = value;
                }
            }
            instance.matrix[15] = 1;
            instance.originalMatrix=instance.matrix;
            if (valid) instances.push_back(instance);
        }
    }
}

std::array<float, 3> storylandCollisionSurfaceColor(uint8_t surface) {
    const float hue = std::fmod(surface * 0.61803398875f + 0.31f, 1.0f) * 6.0f;
    const int sector = int(hue);
    const float fraction = hue - sector, low = 0.24f;
    const float down = 1.0f - (1.0f-low)*fraction, up = low + (1.0f-low)*fraction;
    switch (sector) {
    case 0: return {1, up, low}; case 1: return {down, 1, low};
    case 2: return {low, 1, up}; case 3: return {low, down, 1};
    case 4: return {up, low, 1}; default: return {1, low, down};
    }
}

std::vector<StorylandNativeTrafficActor> storylandCreateNativeTraffic(
    const StorylandNativePaths& paths, const StorylandNativePoint& origin, bool vehicle, size_t maximum,
    float minimumDistance, float maximumDistance, bool water) {
    if (maximum == 0) return {};
    minimumDistance = std::max(0.0f, minimumDistance);
    maximumDistance = std::max(minimumDistance, maximumDistance);
    const float minimumSquared = minimumDistance * minimumDistance;
    const float maximumSquared = maximumDistance * maximumDistance;
    std::vector<std::pair<float, uint32_t>> candidates;
    for (uint32_t index = 0; index < paths.nodes.size(); ++index) {
        const auto& node = paths.nodes[index];
        if (node.vehicle != vehicle || node.water != water || node.disabled) continue;
        const float dx = origin.x - node.position.x;
        const float dy = origin.y - node.position.y;
        const float dz = origin.z - node.position.z;
        const float squaredDistance = dx * dx + dy * dy + dz * dz;
        if (squaredDistance < minimumSquared || squaredDistance > maximumSquared ||
            chooseNext(paths, index, UINT32_MAX, vehicle) == UINT32_MAX) continue;
        candidates.emplace_back(squaredDistance, index);
    }
    std::sort(candidates.begin(), candidates.end());
    std::vector<StorylandNativeTrafficActor> actors;
    for (float minimumSeparation : {vehicle ? 26.0f : 12.0f, 12.0f}) {
        for (const auto& candidate : candidates) {
            const auto& point = paths.nodes[candidate.second].position;
            if (std::any_of(actors.begin(), actors.end(), [&](const auto& actor) {
                return pointDistance(actor.position, point) < minimumSeparation;
            })) continue;
            StorylandNativeTrafficActor actor;
            actor.node = candidate.second;
            actor.routeSeed = uint32_t(actors.size()+1) * 0x9E3779B9u ^ actor.node;
            actor.nextNode = chooseNext(paths, actor.node, UINT32_MAX, vehicle, actor.routeSeed);
            actor.vehicle = vehicle;
            actor.water = water;
            // Leeds path nodes describe road centerlines. Drive on the right
            // side of each directed edge, including reciprocal road links.
            actor.laneOffset=vehicle&&!water?1.35f:0.0f;
            if(vehicle&&!water) {
                const auto* link=roadLink(paths,actor.node,actor.nextNode);
                if(link&&link->nativeLanes&&link->lanes)actor.lane=actor.routeSeed%link->lanes;
            }
            actor.position=lanePosition(paths,actor.node,actor.nextNode,0.0f,actor.laneOffset,0.0f,UINT32_MAX,actor.lane);
            const auto& destination = paths.nodes[actor.nextNode].position;
            actor.yaw = std::atan2(destination.y-point.y,destination.x-point.x)*57.295779513f;
            actors.push_back(actor);
            if (actors.size() >= maximum) break;
        }
        if (actors.size() >= maximum) break;
    }
    return actors;
}

bool storylandJoinNativeVehicleRoute(const StorylandNativePaths& paths,
    StorylandNativeTrafficActor& actor, const StorylandNativeZones* zones,
    bool avoidLevelTransitions) {
    float nearest = std::numeric_limits<float>::infinity();
    uint32_t from = UINT32_MAX;
    uint32_t to = UINT32_MAX;
    float fraction = 0;
    float bestHeading=-2;
    const float angle=actor.yaw*.01745329251994329577f;
    const float headingX=std::cos(angle),headingY=std::sin(angle);
    const bool avoid=avoidLevelTransitions||actor.avoidLevelTransitions;
    for (uint32_t node = 0; node < paths.nodes.size(); ++node) {
        const auto& source = paths.nodes[node];
        if (!source.vehicle || source.water != actor.water || source.disabled) continue;
        // Rejoining must examine every legal edge. Choosing a random next node
        // here can omit the road directly under the car at a junction.
        for(const auto& link:source.links) {
            const uint32_t next=link.target;
            if(next>=paths.nodes.size()||(!source.water&&link.nativeLanes&&!link.lanes))continue;
            const auto& targetNode=paths.nodes[next];
            if(!targetNode.vehicle||targetNode.water!=actor.water||targetNode.disabled)continue;
            if(avoid) {
                if(paths.vcs&&zones) {
                    const auto* first=zones->at(source.position,false);
                    const auto* second=zones->at(targetNode.position,false);
                    if(first&&second&&first->level!=second->level)continue;
                } else if(!paths.vcs&&(targetNode.flags&0x40u))continue;
            }
            const auto& target=targetNode.position;
            const float dx=target.x-source.position.x,dy=target.y-source.position.y,dz=target.z-source.position.z;
            const float square=dx*dx+dy*dy+dz*dz;
            if(square<.0001f)continue;
            const float along=std::clamp(((actor.position.x-source.position.x)*dx+
                (actor.position.y-source.position.y)*dy+(actor.position.z-source.position.z)*dz)/square,0.0f,1.0f);
            const StorylandNativePoint projected{source.position.x+dx*along,
                source.position.y+dy*along,source.position.z+dz*along};
            const float score=pointDistance(projected,actor.position);
            const float horizontal=std::hypot(dx,dy);
            const float heading=horizontal>.001f?(headingX*dx+headingY*dy)/horizontal:0;
            if(score<nearest-.001f||(std::fabs(score-nearest)<=.001f&&heading>bestHeading)) {
                nearest=score;bestHeading=heading;from=node;to=next;fraction=along;
            }
        }
    }
    if (from == UINT32_MAX) return false;
    actor.previousNode = UINT32_MAX;
    actor.node = from;
    actor.nextNode = to;
    actor.distance = pointDistance(paths.nodes[from].position, paths.nodes[to].position) * fraction;
    actor.groundOffset = actor.position.z - (paths.nodes[from].position.z * (1 - fraction) +
        paths.nodes[to].position.z * fraction);
    const auto* link=roadLink(paths,from,to);
    if(!actor.water&&link&&link->nativeLanes&&link->lanes) {
        float nearestLane=std::numeric_limits<float>::infinity();
        for(uint8_t lane=0;lane<link->lanes;++lane) {
            const auto position=lanePosition(paths,from,to,fraction,actor.laneOffset,actor.groundOffset,
                UINT32_MAX,lane);
            const float separation=pointDistance(position,actor.position);
            if(separation<nearestLane) {nearestLane=separation;actor.lane=lane;}
        }
    }
    actor.destinationRouteValid = false;
    actor.destinationRouteFinished = false;
    actor.destinationRoute.clear();
    actor.destinationRouteCursor = 0;
    actor.vehicle = true;
    return true;
}

bool storylandNativeVehiclePathCoordinate(const StorylandNativePaths& paths,
    const StorylandNativeTrafficActor& actor, int32_t index, StorylandNativePoint& point,
    const StorylandNativeZones* zones, bool avoidLevelTransitions) {
    if (index < 0 || index > 3 || !actor.vehicle || actor.water) return false;
    uint32_t from = actor.node;
    uint32_t to = actor.nextNode;
    uint32_t seed = actor.routeSeed;
    for (int32_t step = 0; step < index; ++step) {
        if (to >= paths.nodes.size()) return false;
        seed = seed * 1664525u + 1013904223u + to;
        const uint32_t next = chooseNext(paths, to, from, true, seed, zones, avoidLevelTransitions);
        from = to;
        to = next;
    }
    if (from >= paths.nodes.size() || to >= paths.nodes.size() || from == to) return false;
    const auto* link = roadLink(paths, from, to);
    if (!link || !link->nativeLanes || !link->lanes) return false;
    const auto& first = paths.nodes[from].position;
    const auto& second = paths.nodes[to].position;
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float length = std::hypot(dx, dy);
    if (length < .001f) return false;
    const float offset = roadOffset(paths, from, to, actor.lane, 0);
    // Retail2D13D0 returns the car-link midpoint plus native lane pitch/divider.
    // It explicitly overwrites the midpoint altitude with zero at2D164C.
    point = {(first.x + second.x) * .5f + dy * offset / length,
        (first.y + second.y) * .5f - dx * offset / length, 0};
    return true;
}

bool storylandNativeTrafficRoutePosition(const StorylandNativePaths& paths,
    const StorylandNativeTrafficActor& actor, StorylandNativePoint& position,
    const StorylandNativeZones* zones, bool avoidLevelTransitions, bool ignoreDisabledRoads) {
    if(actor.node>=paths.nodes.size()||actor.nextNode>=paths.nodes.size()||
        actor.node==actor.nextNode||!std::isfinite(actor.distance)||!std::isfinite(actor.groundOffset))
        return false;
    const float length=pointDistance(paths.nodes[actor.node].position,paths.nodes[actor.nextNode].position);
    if(!std::isfinite(length)||length<.001f)return false;
    uint32_t following=chooseNext(paths,actor.nextNode,actor.node,actor.vehicle,
        actor.routeSeed*1664525u+1013904223u+actor.nextNode,zones,
        avoidLevelTransitions||actor.avoidLevelTransitions,ignoreDisabledRoads);
    if(actor.destinationRouteValid&&!actor.destinationRouteFinished)
        following=actor.destinationRouteCursor+1<actor.destinationRoute.size()?
            actor.destinationRoute[actor.destinationRouteCursor+1]:actor.nextNode;
    position=lanePosition(paths,actor.node,actor.nextNode,std::clamp(actor.distance/length,0.0f,1.0f),
        actor.vehicle&&!actor.water?actor.laneOffset:0,actor.groundOffset,
        actor.previousNode,actor.lane,following);
    return std::isfinite(position.x)&&std::isfinite(position.y)&&std::isfinite(position.z);
}

void storylandAdvanceNativeTraffic(const StorylandNativePaths& paths, StorylandNativeTrafficActor& actor, float seconds,
    const StorylandNativeZones* zones, bool avoidLevelTransitions, bool fastLane,
    bool ignoreDisabledRoads) {
    if (!std::isfinite(seconds) || seconds <= 0 || actor.node >= paths.nodes.size() || actor.nextNode >= paths.nodes.size()) return;
    float travel = std::min(seconds, 0.3f) * (actor.vehicle ? 8.0f : 1.25f);
    if (fastLane && actor.vehicle && !actor.water) actor.lane = 0;
    for (size_t step = 0; step < 32 && travel > 0; ++step) {
        const auto& from = paths.nodes[actor.node].position;
        const auto& to = paths.nodes[actor.nextNode].position;
        const float length = pointDistance(from, to);
        if (length < 0.001f) return;
        uint32_t following=chooseNext(paths,actor.nextNode,actor.node,actor.vehicle,
            actor.routeSeed*1664525u+1013904223u+actor.nextNode,zones,
            avoidLevelTransitions||actor.avoidLevelTransitions,ignoreDisabledRoads);
        if (actor.destinationRouteValid && !actor.destinationRouteFinished) {
            following = actor.destinationRouteCursor+1 < actor.destinationRoute.size() ?
                actor.destinationRoute[actor.destinationRouteCursor+1] : actor.nextNode;
        }
        const auto reference=lanePosition(paths,actor.node,actor.nextNode,std::min(actor.distance/length,1.0f),
            actor.vehicle&&!actor.water?actor.laneOffset:0,actor.groundOffset,actor.previousNode,actor.lane,following);
        const float advance = std::min(travel, std::max(0.0f, length - actor.distance));
        actor.distance += advance;
        travel -= advance;
        const float ratio = std::min(actor.distance / length, 1.0f);
        StorylandNativePoint target=lanePosition(paths,actor.node,actor.nextNode,ratio,
            actor.vehicle&&!actor.water?actor.laneOffset:0.0f,actor.groundOffset,actor.previousNode,
            actor.lane,following);
        // Preserve contact recovery/avoidance displacement rather than snapping
        // the next frame straight back into the obstructed centerline.
        const float offsetX=actor.position.x-reference.x,offsetY=actor.position.y-reference.y;
        const float offLength=std::hypot(offsetX,offsetY);
        const float recovery=actor.vehicle&&actor.blockedSeconds>0.0f?0.0f:0.15f;
        const float retain=offLength>0?std::max(0.0f,1.0f-advance*recovery/offLength):0;
        target.x+=offsetX*retain;target.y+=offsetY*retain;
        const float desiredYaw=std::atan2(to.y-from.y,to.x-from.x)*57.295779513f;
        const float dx=target.x-actor.position.x,dy=target.y-actor.position.y;
        if(actor.vehicle&&dx*dx+dy*dy>1e-8f)
            actor.yaw=std::atan2(dy,dx)*57.295779513f;
        else if(!actor.vehicle) actor.yaw=desiredYaw;
        actor.position=target;
        if (actor.distance + 0.0001f < length) break;
        actor.crossingCommitted=false;
        actor.previousNode = actor.node;
        actor.node = actor.nextNode;
        actor.routeSeed = actor.routeSeed * 1664525u + 1013904223u + actor.node;
        if (actor.destinationRouteValid && !actor.destinationRouteFinished) {
            if (actor.destinationRouteCursor >= actor.destinationRoute.size() ||
                actor.destinationRoute[actor.destinationRouteCursor] != actor.node) {
                actor.destinationRouteValid = false;
                actor.nextNode = actor.node;
                actor.distance = 0;
                return;
            }
            ++actor.destinationRouteCursor;
            if (actor.destinationRouteCursor == actor.destinationRoute.size()) {
                actor.destinationRouteFinished = true;
                actor.nextNode = actor.node;
                actor.distance = 0;
                return;
            }
            const uint32_t next = actor.destinationRoute[actor.destinationRouteCursor];
            if (!storylandVehicleRouteEdgeAllowed(paths,actor.node,next,zones,
                avoidLevelTransitions||actor.avoidLevelTransitions,nullptr,ignoreDisabledRoads)) {
                actor.destinationRouteValid = false;
                actor.nextNode = actor.node;
                actor.distance = 0;
                return;
            }
            actor.nextNode = next;
        } else {
            actor.nextNode = chooseNext(paths, actor.node, actor.previousNode, actor.vehicle, actor.routeSeed,
                zones,avoidLevelTransitions||actor.avoidLevelTransitions,ignoreDisabledRoads);
        }
        actor.distance = 0;
        if (actor.nextNode == UINT32_MAX) { actor.nextNode = actor.node; return; }
    }
}

float storylandNativeTrafficTurn(const StorylandNativePaths& paths,const StorylandNativeTrafficActor& actor,
    float& remaining,bool ignoreDisabledRoads) {
    remaining=0;if(actor.node>=paths.nodes.size()||actor.nextNode>=paths.nodes.size())return 0;
    const auto& a=paths.nodes[actor.node].position;const auto& b=paths.nodes[actor.nextNode].position;
    const float length=pointDistance(a,b);remaining=std::max(0.0f,length-actor.distance);
    const auto next=actor.destinationRouteValid&&!actor.destinationRouteFinished ?
        (actor.destinationRouteCursor+1<actor.destinationRoute.size() ?
            actor.destinationRoute[actor.destinationRouteCursor+1] : actor.nextNode) :
        chooseNext(paths,actor.nextNode,actor.node,actor.vehicle,
            actor.routeSeed*1664525u+1013904223u+actor.nextNode,nullptr,
            actor.avoidLevelTransitions,ignoreDisabledRoads);
    if(next>=paths.nodes.size()||next==actor.nextNode)return 0;
    const auto& c=paths.nodes[next].position;
    const float x=b.x-a.x,y=b.y-a.y,xx=c.x-b.x,yy=c.y-b.y;
    const float denominator=std::hypot(x,y)*std::hypot(xx,yy);
    return denominator>.001f?std::acos(std::clamp((x*xx+y*yy)/denominator,-1.0f,1.0f)):0;
}

bool storylandFindPlayTestDtz(const std::wstring& worldPath, std::wstring& dtzPath, std::string& error) {
    dtzPath.clear();
    error.clear();
    if (worldPath.empty()) { error = "Open a Stories world or GAME.DTZ first."; return false; }
    const auto dtz = storylandFindNativeGameFile(worldPath, "GAME.DTZ");
    if (!dtz.empty()) { dtzPath = dtz.wstring(); return true; }
    const auto raw = storylandFindNativeGameFile(worldPath, "GAME.BIN");
    if (!raw.empty()) { dtzPath = raw.wstring(); return true; }
    error = "GAME.DTZ was not found in this game's runtime folders.";
    return false;
}

std::vector<StorylandEmbeddedCollision> storylandDecodeEmbeddedMdlCollision(const std::vector<uint8_t>& bytes) {
    std::vector<StorylandEmbeddedCollision> result;
    if (!contains(bytes,0,36) || read32(bytes,0) != 0x006D646C) return result;
    const uint32_t limit = read32(bytes,12), table = read32(bytes,16), count = read32(bytes,20);
    if (limit < 36 || limit > bytes.size() || table < limit || count > 1000000 || !contains(bytes,table,count,4)) return result;
    std::set<uint32_t> fields, targets;
    for (uint32_t index=0;index<count;++index) {
        const uint32_t field = read32(bytes,size_t(table)+index*4);
        if (field >= 32 && uint64_t(field)+4 <= limit) { fields.insert(field); targets.insert(read32(bytes,field)); }
    }
    for (uint32_t offset : targets) {
        if (offset < 32 || uint64_t(offset)+96 > limit || (offset & 15)) continue;
        for (bool vcs : {true,false}) {
            StorylandNativeCollisionModel model;
            std::string error;
            if (!storylandDecodeNativeCollisionModel(bytes,offset,limit,vcs,model,error) ||
                (model.triangles.empty() && model.boxes.empty() && model.spheres.empty())) continue;
            const uint32_t pointers = offset+(vcs?0x38:0x3C);
            if ((!model.spheres.empty()&&!fields.count(pointers)) ||
                (!model.boxes.empty()&&!fields.count(pointers+8)) ||
                (!model.triangles.empty()&&(!fields.count(pointers+16)||!fields.count(pointers+20)))) continue;
            result.push_back({offset,std::move(model)}); break;
        }
    }
    return result;
}

bool storylandNativeWaterHeight(const StorylandNativeWorld& world,float x,float y,float& height) {
    if(!std::isfinite(x)||!std::isfinite(y)) return false;
    bool found=false;
    for(const auto& region:world.water) {
        if(x>=region.west&&x<=region.east&&y>=region.south&&y<=region.north) {
            if(!found||region.height>height) height=region.height;
            found=true;
        }
    }
    return found;
}
bool storylandNativeWaterSegment(const StorylandNativeWorld& world,
    const StorylandNativePoint& from,const StorylandNativePoint& to) {
    if(!finitePoint(from)||!finitePoint(to)) return false;
    std::vector<std::pair<float,float>> intervals;
    for(const auto& region:world.water) {
        if(std::fabs(from.z-region.height)>1.5f||std::fabs(to.z-region.height)>1.5f) continue;
        float low=0,high=1;
        const auto clip=[&](float origin,float direction,float minimum,float maximum) {
            if(std::fabs(direction)<1e-7f) return origin>=minimum&&origin<=maximum;
            float first=(minimum-origin)/direction,last=(maximum-origin)/direction;
            if(first>last) std::swap(first,last);
            low=std::max(low,first);high=std::min(high,last);
            return low<=high;
        };
        if(clip(from.x,to.x-from.x,region.west,region.east)&&
           clip(from.y,to.y-from.y,region.south,region.north)) intervals.emplace_back(low,high);
    }
    std::sort(intervals.begin(),intervals.end());
    float covered=0;
    for(const auto& interval:intervals) {
        if(interval.first>covered+1e-6f) return false;
        covered=std::max(covered,interval.second);
        if(covered>=1) return true;
    }
    return false;
}
void storylandPrepareNativeWater(const std::vector<uint8_t>& data,bool vcs,StorylandNativeWorld& world) {
    world.water.clear();
    const auto disableWater=[&]() {
        for(auto& node:world.paths.nodes) if(node.water) node.disabled=true;
    };
    const size_t slot=vcs?0xA8:0xA4;
    if(!contains(data,slot,4)) {disableWater();return;}
    const uint32_t block=read32(data,slot);
    if(!block||!contains(data,block,12)) {disableWater();return;}
    const uint32_t count=read32(data,block),levels=read32(data,block+4),rectangles=read32(data,block+8);
    if(count>48||!levels||!rectangles||!contains(data,levels,count,4)||!contains(data,rectangles,count,16)) {disableWater();return;}
    for(uint32_t index=0;index<count;++index) {
        const size_t bounds=size_t(rectangles)+size_t(index)*16;
        StorylandNativeWater region;
        region.west=readFloat(data,bounds);region.north=readFloat(data,bounds+4);
        region.east=readFloat(data,bounds+8);region.south=readFloat(data,bounds+12);
        region.height=readFloat(data,size_t(levels)+size_t(index)*4);
        if(finitePoint({region.west,region.south,region.height})&&
           finitePoint({region.east,region.north,region.height})&&region.west<region.east&&region.south<region.north)
            world.water.push_back(region);
    }
    if(world.water.empty()) {disableWater();return;}
    for(auto& node:world.paths.nodes) {
        float height=0;
        const bool onWater=storylandNativeWaterHeight(world,node.position.x,node.position.y,height)&&
            std::fabs(node.position.z-height)<1.5f;
        if(vcs) node.water=node.vehicle&&onWater;
        if(node.water&&!onWater) {node.disabled=true;continue;}
        if(node.water) {
            const StorylandNativePoint top{node.position.x,node.position.y,height+0.5f};
            const StorylandNativePoint bottom{top.x,top.y,height-1.0f};
            if(storylandTraceNativeSegment(world,top,bottom)<1) {
                if(vcs) node.water=false;
                else node.disabled=true;
            } else node.position.z=height;
        }
    }
    for(auto& node:world.paths.nodes) {
        if(!node.water) continue;
        node.links.erase(std::remove_if(node.links.begin(),node.links.end(),[&](const auto& link) {
            if(link.target>=world.paths.nodes.size()) return true;
            const auto& target=world.paths.nodes[link.target];
            if(!target.water||target.disabled||!storylandNativeWaterSegment(world,node.position,target.position)) return true;
            auto from=node.position,to=target.position;from.z+=0.6f;to.z+=0.6f;
            return storylandTraceNativeSegment(world,from,to)<1;
        }),node.links.end());
    }
}

namespace {
template<class IsMarked,class Mark>
void markLevelTransitions(const StorylandNativePaths& paths,const std::array<float,6>& b,
    IsMarked isMarked,Mark mark) {
    if(!std::all_of(b.begin(),b.end(),[](float x){return std::isfinite(x);}))return;
    std::vector<uint32_t> pending;
    const auto degree=[](const StorylandNativePathNode& node) {
        // Keep native junctions intact when unusable links have been removed.
        return std::max(node.links.size(),size_t(node.flags&0xFu));
    };
    for(uint32_t index=0;index<paths.nodes.size();++index) {
        const auto& p=paths.nodes[index].position;
        if(!(p.x>std::min(b[0],b[3])&&p.x<std::max(b[0],b[3])&&
            p.y>std::min(b[1],b[4])&&p.y<std::max(b[1],b[4])&&
            p.z>std::min(b[2],b[5])&&p.z<std::max(b[2],b[5])))continue;
        mark(index);pending.push_back(index);
        while(!pending.empty()) {
            const uint32_t current=pending.back();pending.pop_back();
            const auto& node=paths.nodes[current];
            if(degree(node)>=3)continue;
            for(const auto& link:node.links) {
                if(link.target>=paths.nodes.size()||isMarked(link.target)||degree(paths.nodes[link.target])>=3)continue;
                mark(link.target);pending.push_back(link.target);
            }
        }
    }
}
template<class ReadFlags,class WriteFlags>
void applyRoadChangeFlags(const StorylandNativePaths& paths,const StorylandRoadChange& change,
    ReadFlags readFlags,WriteFlags writeFlags) {
    const auto& b=change.bounds;
    if(!std::all_of(b.begin(),b.end(),[](float value){return std::isfinite(value);}))return;
    if(change.mode==StorylandRoadChange::Mode::LevelTransitions) {
        markLevelTransitions(paths,b,[&](uint32_t index){return (readFlags(index)&0x40u)!=0;},
            [&](uint32_t index){writeFlags(index,uint16_t(readFlags(index)|0x40u));});
        return;
    }
    const bool angled=change.angled||change.mode==StorylandRoadChange::Mode::AngledAvailability;
    float baseline=0,alongX=0,alongY=0,sideX=0,sideY=0;
    if(angled) {
        float x=b[3]-b[0],y=b[4]-b[1];baseline=std::hypot(x,y);
        if(!std::isfinite(change.width)||!std::isfinite(baseline)||baseline==0||change.width<0)return;
        alongX=x/baseline;alongY=y/baseline;
        // Match CGeneral's heading convention, including its horizontal-edge
        // epsilon, before constructing the signed side of the road area.
        constexpr float pi=3.14159265358979323846f;
        if(y==0)y=.0001f;
        const float tangent=std::atan2(x/y,1.0f);
        float angle=x>0?(y>0?pi-tangent:-tangent):(y>0?-(pi+tangent):-tangent);
        angle+=pi*.5f;
        while(angle<0)angle+=2*pi;
        while(angle>2*pi)angle-=2*pi;
        sideX=std::sin(angle)*change.width;sideY=-std::cos(angle)*change.width;
        const float sideLength=std::hypot(sideX,sideY);
        if(sideLength==0)sideX=sideY=std::numeric_limits<float>::quiet_NaN();
        else {sideX/=sideLength;sideY/=sideLength;}
        if(paths.vcs) {
            sideX=alongY;
            sideY=-alongX;
        }
    }
    const auto inside=[&](const StorylandNativePoint& p) {
        if(p.z<std::min(b[2],b[5])||p.z>std::max(b[2],b[5]))return false;
        if(!angled)return p.x>=std::min(b[0],b[3])&&p.x<=std::max(b[0],b[3])&&
            p.y>=std::min(b[1],b[4])&&p.y<=std::max(b[1],b[4]);
        const float x=p.x-b[0],y=p.y-b[1],along=x*alongX+y*alongY,side=x*sideX+y*sideY;
        return !(along<0||along>baseline||side<0||side>change.width);
    };
    const bool disabled=!change.enabled;
    const auto already=[&](uint32_t index){return bool(readFlags(index)&0x20u)==disabled;};
    const auto update=[&](uint32_t index) {
        const uint16_t flags=readFlags(index);
        writeFlags(index,disabled?uint16_t(flags|0x20u):uint16_t(flags&~0x20u));
    };
    const auto degree=[](const StorylandNativePathNode& node) {
        return std::max(node.links.size(),size_t(node.flags&0xFu));
    };
    std::vector<uint32_t> pending;
    for(uint32_t index=0;index<paths.nodes.size();++index) {
        if(paths.nodes[index].vehicle!=change.vehicle||!inside(paths.nodes[index].position)||already(index))continue;
        update(index);
        if(paths.vcs)continue;
        pending.push_back(index);
        while(!pending.empty()) {
            const uint32_t current=pending.back();pending.pop_back();
            if(degree(paths.nodes[current])>=3)continue;
            for(const auto& link:paths.nodes[current].links) {
                if(link.target>=paths.nodes.size()||already(link.target)||degree(paths.nodes[link.target])>=3)continue;
                update(link.target);pending.push_back(link.target);
            }
        }
    }
}
}
std::vector<uint16_t> storylandNativeRoadNodeFlags(const StorylandNativePaths& paths,
    const std::vector<StorylandRoadChange>& changes) {
    std::vector<uint16_t> flags(paths.nodes.size());
    for(size_t index=0;index<paths.nodes.size();++index)
        flags[index]=uint16_t((paths.nodes[index].flags&~0x20u)|(paths.nodes[index].disabled?0x20u:0u));
    for(const auto& change:changes)applyRoadChangeFlags(paths,change,
        [&](uint32_t index){return flags[index];},[&](uint32_t index,uint16_t value){flags[index]=value;});
    return flags;
}
std::vector<uint8_t> storylandNativeLevelTransitionNodes(const StorylandNativePaths& paths,
    const std::vector<StorylandRoadChange>& changes) {
    const auto flags=storylandNativeRoadNodeFlags(paths,changes);
    std::vector<uint8_t> marked(paths.nodes.size());
    for(size_t index=0;index<paths.nodes.size();++index)marked[index]=(flags[index]&0x40u)!=0;
    return marked;
}

size_t storylandApplyNativeRoadChanges(StorylandNativePaths& paths,
    const std::vector<StorylandRoadChange>& changes,size_t applied) {
    if(applied>changes.size())return changes.size();
    for(size_t i=applied;i<changes.size();++i)applyRoadChangeFlags(paths,changes[i],
        [&](uint32_t index){return uint16_t((paths.nodes[index].flags&~0x20u)|(paths.nodes[index].disabled?0x20u:0u));},
        [&](uint32_t index,uint16_t flags){paths.nodes[index].flags=flags;paths.nodes[index].disabled=(flags&0x20u)!=0;});
    return changes.size();
}
