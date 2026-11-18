#pragma once
#include "src/storyland/storyland_mission_vm.h"
#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iterator>

inline const char* storylandVcsRadarSpriteName(int32_t sprite) {
    // SLUS_215.90 radar initialization: 0x110AB8; sprite table: 0x4D3D88.
    static constexpr const char* names[]={
        "",
        "radar_centre",
        "arrow",
        "radar_north",
        "gun",
        "hardware",
        "radar_save",
        "property",
        "spray",
        "phone",
        "radar_bomb",
        "radar_cshop",
        "radar_powerup",
        "mp_base",
        "mp_checkpoint",
        "mp_player",
        "mp_objective",
        "mp_car",
        "mp_tank",
        "mp_carlockup",
        "mp_targetplayer",
        "arrow_00",
        "arrow_01",
        "arrow_02",
        "arrow_03",
        "arrow_04",
        "arrow_05",
        "arrow_06",
        "arrow_07",
        "radar_empire",
        "radar_empire_dam",
        "radar_empire_con",
        "emipreattack01",
        "emipreattack02",
        "emipreattack03",
        "radar_bryan",
        "radar_gonz",
        "radar_lance",
        "radar_louise",
        "radar_marty",
        "radar_phil",
        "radar_reni",
        "radar_diaz",
        "radar_jerry",
        "radar_mendez",
        "radar_umberto",
        "radar_plane",
        "radar_gunshop",
        "radar_usj",
        "radar_rampage",
        "radar_balloon",
        "radar_bcase",
        "radar_mpbomb",
        "radar_boat",
        "radar_heli",
    };
    return sprite>=0&&size_t(sprite)<sizeof(names)/sizeof(names[0])?names[sprite]:"";
}

inline const char* storylandLcsRadarSpriteName(int32_t sprite) {
    // SLUS_214.23 0x11556C initializes the 0x3D9470 sprite array.
    static constexpr const char* names[]={
        "", "radar_centre", "arrow", "radar_north", "radar_avery", "radar_biker",
        "radar_cortez", "radar_diaz", "radar_kent", "radar_lawyer", "radar_phil",
        "bikers", "boatyard", "club", "cubans", "filmstudio", "gun", "haitians",
        "hardware", "radar_save", "radar_strip", "icecream", "kcabs", "lovefist",
        "printworks", "property", "sunyard", "spray", "tshirt", "tommy", "phone",
        "radar_8ball", "radar_burner", "radar_cipriani", "radar_donald", "radar_joseph",
        "radar_leone", "radar_maria", "radar_mcray", "radar_toshiko", "radar_vincenzo",
        "radar_bomb", "radar_cshop", "radar_propertyr", "radar_flag", "radar_powerup",
        "mp_base", "mp_checkpoint", "mp_player", "mp_objective", "mp_car", "mp_tank",
        "mp_carlockup", "mp_targetplayer", "arrow_00", "arrow_01", "arrow_02", "arrow_03",
        "arrow_04", "arrow_05", "arrow_06", "arrow_07"
    };
    return sprite>=0&&size_t(sprite)<std::size(names)?names[sprite]:"";
}

inline const char* storylandRadarSpriteName(bool vcs,int32_t sprite) {
    return vcs?storylandVcsRadarSpriteName(sprite):storylandLcsRadarSpriteName(sprite);
}

inline bool storylandMissionRadarTraceDisplayed(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::RadarBlip& blip,bool vcs,bool pauseMap) {
    if(!vcs) {
        using Entity=StorylandMissionEnvironment::BlipEntity;
        if(pauseMap&&world.lcsExtendedWorld.playerInStadium)return false;
        if(blip.sprite==25&&(blip.entity==Entity::Coordinate||blip.entity==Entity::ContactPoint))return false;
    }
    if(!vcs&&blip.entity==StorylandMissionEnvironment::BlipEntity::ContactPoint&&
        world.lcsMissionFlagSet&&world.lcsMissionFlagSet())return false;
    // VCS 0x1136F4..0x113704: +0x1AD9 is the radar's pause-map flag.
    return (blip.display&2)!=0&&!(vcs&&pauseMap&&
        (world.playerInStadium||(blip.sprite>=29&&blip.sprite<=31)));
}

inline bool storylandRadarFinitePoint(const StorylandNativePoint& point) {
    return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
}

inline bool storylandMissionBlipEntityExists(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::RadarBlip& blip,bool vcs=true) {
    using Entity=StorylandMissionEnvironment::BlipEntity;
    switch(blip.entity) {
    case Entity::Coordinate:
    case Entity::ContactPoint:return true;
    case Entity::Character:
        if(blip.entityHandle==0||(!vcs&&blip.entityHandle==1))return true;
        {
            const auto found=world.missionCharacters.find(blip.entityHandle);
            return found!=world.missionCharacters.end()&&!found->second.removeFromWorld;
        }
    case Entity::Vehicle:
        {
            const auto found=world.missionVehicles.find(blip.entityHandle);
            return found!=world.missionVehicles.end()&&!found->second.deleted;
        }
    case Entity::Object:
        return std::any_of(world.objects.begin(),world.objects.end(),
            [&](const auto& object){return object.handle==blip.entityHandle;});
    case Entity::Pickup:
        return std::any_of(world.pickups.begin(),world.pickups.end(),
            [&](const auto& pickup){return pickup.handle==blip.entityHandle&&!pickup.removed;});
    }
    return false;
}

inline bool storylandMissionBlipPosition(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::RadarBlip& blip,StorylandNativePoint& position,bool vcs=true) {
    using Entity=StorylandMissionEnvironment::BlipEntity;
    if(!storylandMissionBlipEntityExists(world,blip,vcs))return false;
    if(blip.entity==Entity::Coordinate||blip.entity==Entity::ContactPoint)position={blip.x,blip.y,blip.z};
    else if(blip.entity==Entity::Character) {
        if(blip.entityHandle==0||(!vcs&&blip.entityHandle==1))position={world.x,world.y,world.z};
        else {
            const auto& character=world.missionCharacters.at(blip.entityHandle);
            const auto vehicle=world.missionVehicles.find(character.vehicle);
            if(character.vehicle>=0&&vehicle!=world.missionVehicles.end()&&!vehicle->second.deleted)
                position={vehicle->second.x,vehicle->second.y,vehicle->second.z};
            else position={character.x,character.y,character.z};
        }
    } else if(blip.entity==Entity::Vehicle) {
        const auto& vehicle=world.missionVehicles.at(blip.entityHandle);
        position={vehicle.x,vehicle.y,vehicle.z};
    } else if(blip.entity==Entity::Object) {
        const auto found=std::find_if(world.objects.begin(),world.objects.end(),
            [&](const auto& object){return object.handle==blip.entityHandle;});
        position={found->x,found->y,found->z};
    } else {
        const auto found=std::find_if(world.pickups.begin(),world.pickups.end(),
            [&](const auto& pickup){return pickup.handle==blip.entityHandle;});
        if(found->collected||found->unavailable)return false;
        position={found->x,found->y,found->z};
    }
    return storylandRadarFinitePoint(position);
}

inline void storylandPruneMissionRadarBlips(StorylandMissionEnvironment& world,bool vcs=true) {
    std::vector<int32_t> removed;
    world.radarBlips.erase(std::remove_if(world.radarBlips.begin(),world.radarBlips.end(),
        [&](const auto& blip) {
            if(storylandMissionBlipEntityExists(world,blip,vcs))return false;
            removed.push_back(blip.handle);
            return true;
        }),world.radarBlips.end());
    world.markers.erase(std::remove_if(world.markers.begin(),world.markers.end(),
        [&](const auto& marker){return std::find(removed.begin(),removed.end(),marker.handle)!=removed.end();}),world.markers.end());
}

inline int32_t storylandAllocateMissionBlipHandle(StorylandMissionEnvironment& world,bool vcs=true) {
    // Both native trace pools contain 75 slots. VCS 0x110E30..0x110EA4 stores
    // a per-slot halfword generation and validates it before accepting a handle.
    storylandPruneMissionRadarBlips(world,vcs);
    std::array<bool,75> occupied{};
    for(const auto& blip:world.radarBlips) {
        const uint32_t handle=uint32_t(blip.handle);
        const size_t slot=handle&0xffffu;
        if(slot>=occupied.size())continue;
        occupied[slot]=true;
        if(world.radarBlipGenerations[slot]==0)
            world.radarBlipGenerations[slot]=uint16_t(handle>>16);
    }
    for(size_t slot=0;slot<occupied.size();++slot) {
        if(occupied[slot])continue;
        auto& generation=world.radarBlipGenerations[slot];
        generation=generation>0xfffdu?1:uint16_t(generation+1);
        const uint32_t handle=(uint32_t(generation)<<16)|uint32_t(slot);
        world.nextBlipHandle=int32_t(handle);
        return int32_t(handle);
    }
    return -1;
}

inline std::array<uint8_t,4> storylandMissionBlipColor(uint32_t color,bool bright,bool vcs=true) {
    // Native packed RGBA: VCS 0x116928, LCS 0x119AA0. Dimming uses a
    // separate palette; it is not a half-intensity version of the bright row.
    static constexpr std::array<uint8_t,4> brightColors[]={
        {255,72,77,255},{95,160,106,255},{72,77,255,255},{225,225,225,255},
        {255,255,0,255},{255,165,201,255},{30,255,255,255},{110,16,60,255}
    };
    static constexpr std::array<uint8_t,4> dimColors[]={
        {127,0,0,255},{0,127,0,255},{0,0,127,255},{127,127,127,255},
        {127,127,0,255},{110,16,60,255},{0,88,88,255},{110,16,60,255}
    };
    if(color>=std::size(brightColors))
        return {uint8_t(color>>24),uint8_t(color>>16),uint8_t(color>>8),uint8_t(color)};
    if(!vcs) {
        if(color==5)return bright?std::array<uint8_t,4>{255,0,255,255}:std::array<uint8_t,4>{127,0,127,255};
        if(color==6)return bright?std::array<uint8_t,4>{0,255,255,255}:std::array<uint8_t,4>{0,127,127,255};
        if(color==7)return {127,0,127,255};
    }
    return bright?brightColors[color]:dimColors[color];
}

inline uint8_t storylandRadarSpriteAlpha(const StorylandNativePoint& target,
    const StorylandNativePoint& player,uint8_t distanceAlpha) {
    // Both native coordinate paths keep sprites opaque across the +/-170-degree
    // heading seam: VCS 0x1116FC..0x111758, LCS 0x115FA8..0x115FE0.
    const float angle=std::atan2(target.x-player.x,player.y-target.y)*57.2957763671875f;
    return angle< -170.0f||angle>170.0f?255:distanceAlpha;
}

inline std::array<uint8_t,4> storylandMissionRadarTint(
    const StorylandMissionEnvironment::RadarBlip& blip,bool vcs,uint8_t spriteAlpha=255) {
    // DrawCoordBlip uses white sprite pixels except for mp_objective. Native
    // trace glyphs and the objective sprite use HUD opacity, not distance alpha.
    const int objectiveSprite=vcs?16:49;
    if(blip.sprite>0&&blip.sprite!=objectiveSprite)return {255,255,255,spriteAlpha};
    auto color=storylandMissionBlipColor(blip.color,blip.bright,vcs);
    color[3]=255;
    return color;
}

enum class StorylandRadarHeight { Above, Below, Level };

inline float storylandNativeRadarRange(bool inVehicle,float speed,bool frozen=false,float overrideRange=0) {
    // VCS 0x110F34..0x11105C. Native
    // velocity units are converted from 50 Hz to the host's metres/second.
    if(frozen)return 256.0f;
    if(std::isfinite(overrideRange)&&overrideRange>0)return overrideRange;
    if(!inVehicle||!std::isfinite(speed))return 96.0f;
    const float blend=std::clamp((std::fabs(speed)-15.0f)/30.0f,0.0f,1.0f);
    return 96.0f+160.0f*blend;
}

inline StorylandRadarHeight storylandRadarHeight(float targetZ,float playerZ) {
    // VCS 0x1117FC..0x11185C, LCS 0x116070..0x1160D4.
    if(targetZ-playerZ>2.0f)return StorylandRadarHeight::Above;
    if(targetZ-playerZ< -4.0f)return StorylandRadarHeight::Below;
    return StorylandRadarHeight::Level;
}

struct StorylandRadarProjection {
    float x=0,y=0,distance=0;
    uint8_t alpha=255;
    bool valid=false,visible=false,atEdge=false;
};

inline StorylandRadarProjection storylandProjectMissionBlip(const StorylandNativePoint& target,
    const StorylandNativePoint& origin,float yaw,float range,bool shortRange,bool pauseMap=false) {
    StorylandRadarProjection result;
    if(!storylandRadarFinitePoint(target)||!storylandRadarFinitePoint(origin)||
        !std::isfinite(yaw)||!std::isfinite(range)||range<=0)return result;
    const float radians=yaw*.01745329251994329577f;
    const float dx=(target.x-origin.x)/range,dy=(target.y-origin.y)/range;
    result.x=std::sin(radians)*dx-std::cos(radians)*dy;
    result.y=-(std::cos(radians)*dx+std::sin(radians)*dy);
    result.distance=std::hypot(result.x,result.y);
    if(!std::isfinite(result.distance))return result;
    result.valid=true;
    // VCS LimitRadarPoint 0x1145E8 returns the pre-clamp distance. The
    // short-range branch at 0x1117A4 permits the exact one-radius boundary.
    result.visible=pauseMap||!shortRange||result.distance<=1.0f;
    result.atEdge=!pauseMap&&result.distance>1.0f;
    if(result.atEdge) {result.x/=result.distance;result.y/=result.distance;}
    // VCS CalculateBlipAlpha 0x114678: 255 at <=1 radius, 128 at >=10.
    if(!pauseMap&&result.distance>1.0f) {
        const float blend=std::clamp((result.distance-1.0f)/9.0f,0.0f,1.0f);
        result.alpha=uint8_t(std::lround(255.0f*(1-blend)+128.0f*blend));
    }
    return result;
}

inline StorylandNativePoint storylandRadarMapPlayer(const StorylandMissionEnvironment& world) {
    const auto& anchor = world.radarMapAnchor;
    const auto& local = world.radarLocalOrigin;
    if (anchor == std::array<float, 3>{}) {
        return {world.x, world.y, world.z};
    }
    if (local == std::array<float, 3>{}) {
        return {anchor[0], anchor[1], anchor[2]};
    }
    return {anchor[0] + world.x - local[0], anchor[1] + world.y - local[1], anchor[2] + world.z - local[2]};
}

inline StorylandNativePoint storylandRadarBlipPoint(const StorylandMissionEnvironment& world,
    const StorylandMissionEnvironment::RadarBlip& blip, StorylandNativePoint point, bool pauseMap) {
    const auto& anchor = world.radarMapAnchor;
    if (anchor == std::array<float, 3>{}) {
        return point;
    }
    if (!pauseMap && blip.exterior) {
        point.x -= anchor[0] - world.x;
        point.y -= anchor[1] - world.y;
        point.z -= anchor[2] - world.z;
    } else if (pauseMap && !blip.exterior) {
        const auto& local = world.radarLocalOrigin;
        const bool localOrigin = local != std::array<float, 3>{};
        point.x += anchor[0] - (localOrigin ? local[0] : world.x);
        point.y += anchor[1] - (localOrigin ? local[1] : world.y);
        // Native 0x111638 subtracts this Z offset, unlike the XY components.
        point.z -= anchor[2] - (localOrigin ? local[2] : world.z);
    }
    return point;
}
