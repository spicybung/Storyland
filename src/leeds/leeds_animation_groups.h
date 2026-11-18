#pragma once
#include "src/leeds/leeds_anim.h"
#include "src/leeds/leeds_native_layout.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <initializer_list>
#include <string>
#include <string_view>

struct StorylandAnimationAssociation {
    std::string name;
    uint16_t flags=0;
};
struct StorylandAnimationGroup {
    std::string name,bank;
    std::map<uint32_t,StorylandAnimationAssociation> associations;
};
struct StorylandAnimationGroups {
    std::map<uint32_t,StorylandAnimationGroup> groups;
    bool vcsLayout=false;
    static std::string key(std::string value) {
        std::transform(value.begin(),value.end(),value.begin(),[](unsigned char ch){return char(std::tolower(ch));});
        return value;
    }
    static bool matchesKey(std::string_view value,std::string_view normalized) {
        if(value.size()!=normalized.size())return false;
        for(size_t index=0;index<value.size();++index)
            if(char(std::tolower(static_cast<unsigned char>(value[index])))!=normalized[index])return false;
        return true;
    }
    bool load(const std::vector<uint8_t>& bytes,bool vcs) {
        groups.clear();
        vcsLayout=vcs;
        if(!vcs)return loadLcs(bytes);
        if(bytes.size()<0x84)return false;
        const auto word=[&](size_t p)->uint32_t {
            if(p>bytes.size()||bytes.size()-p<4)return 0;
            return uint32_t(bytes[p])|(uint32_t(bytes[p+1])<<8)|(uint32_t(bytes[p+2])<<16)|(uint32_t(bytes[p+3])<<24);
        };
        const auto text=[&](size_t p,size_t length) {
            std::string value;
            for(size_t i=0;i<length&&p+i<bytes.size()&&bytes[p+i];++i) {
                if(bytes[p+i]<32||bytes[p+i]>126)return std::string{};
                value.push_back(char(bytes[p+i]));
            }
            return key(value);
        };
        const uint32_t manager=word(0x80);
        const size_t associations=size_t(manager)+200u*0x34u;
        const size_t counts=associations+990u*0x38u;
        if(!manager||counts+12>bytes.size())return false;
        const uint32_t groupCount=word(counts),associationCount=word(counts+4);
        if(!groupCount||groupCount>200||associationCount>990)return false;
        for(uint32_t index=0;index<groupCount;++index) {
            const size_t row=size_t(manager)+size_t(index)*0x34;
            StorylandAnimationGroup group;
            group.name=text(row,24);group.bank=text(row+24,20);
            const uint32_t first=word(row+44),count=word(row+48);
            if(group.name.empty()||group.bank.empty()||uint64_t(first)+count>associationCount)continue;
            for(uint32_t clip=first;clip<first+count;++clip) {
                const size_t entry=associations+size_t(clip)*0x38;
                const auto name=text(entry+4,44);
                if(!name.empty())group.associations.emplace(word(entry),StorylandAnimationAssociation{name,uint16_t(word(entry+52))});
            }
            groups.emplace(index,std::move(group));
        }
        return !groups.empty();
    }
    bool loadLcs(const std::vector<uint8_t>& bytes) {
        groups.clear();vcsLayout=false;
        const auto fits=[&](size_t offset,size_t count,size_t stride=1) {
            return offset<=bytes.size()&&count<=(bytes.size()-offset)/stride;
        };
        const auto word=[&](size_t offset)->uint32_t {
            if(!fits(offset,4))return 0;
            return uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|
                (uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);
        };
        const auto text=[&](size_t offset,size_t length) {
            std::string value;
            if(!fits(offset,length))return value;
            for(size_t index=0;index<length&&bytes[offset+index];++index) {
                if(bytes[offset+index]<32||bytes[offset+index]>126)return std::string{};
                value.push_back(char(bytes[offset+index]));
            }
            return key(value);
        };
        const uint32_t manager=word(0x80);
        if(manager<32||!fits(manager,20))return false;
        const uint32_t table=word(size_t(manager)+16);
        if(table<32||!fits(table,84,20))return false;
        // LCS serializes CAnimBlendAssocGroup[84], with 0x44-byte live
        // associations. VCS's inline definition arrays have another layout.
        for(uint32_t groupId=0;groupId<84;++groupId) {
            const size_t row=size_t(table)+groupId*20;
            const uint32_t block=word(row),list=word(row+4),count=word(row+8);
            if(word(row+16)!=groupId||!block||!list||!count||count>512||
                !fits(block,40)||!fits(list,count,0x44))continue;
            StorylandAnimationGroup group;
            group.name=group.bank=text(block,20);
            if(group.bank.empty())continue;
            for(uint32_t index=0;index<count;++index) {
                const size_t association=size_t(list)+index*0x44;
                const uint32_t tree=word(association+0x20);
                if(tree<32||!fits(tree,36))continue;
                const auto name=text(size_t(tree)+4,24);
                const uint32_t ids=word(association+0x30);
                if(name.empty()||(ids>>16)!=groupId)continue;
                group.associations.emplace(ids&0xFFFF,
                    StorylandAnimationAssociation{name,uint16_t(word(association+8))});
            }
            if(!group.associations.empty())groups.emplace(groupId,std::move(group));
        }
        StorylandNativeAnimationLayout layout;
        if(!storylandNativeAnimationLayout(bytes,false,layout))return false;
        std::map<std::string,std::set<std::string>> declaredBanks;
        for(uint32_t index=0;index<layout.blockCount;++index) {
            const size_t row=size_t(layout.blocks)+size_t(index)*layout.blockStride;
            const auto bank=text(row,20);
            const uint32_t first=word(row+layout.firstTreeOffset),count=word(row+layout.treeCountOffset);
            if(bank.empty()||uint64_t(first)+count>layout.treeCount)continue;
            auto& names=declaredBanks[bank];
            for(uint32_t clip=first;clip<first+count;++clip) {
                const auto name=text(size_t(layout.trees)+size_t(clip)*layout.treeStride+4,24);
                if(!name.empty())names.insert(name);
            }
        }
        const auto restoreGroup=[&](uint32_t id,uint32_t first,StorylandAnimationGroup group) {
            const size_t row=size_t(table)+size_t(id)*20;
            if(groups.count(id)||word(row+16)!=id||word(row+12)!=first)return;
            const auto declared=declaredBanks.find(group.bank);
            if(declared==declaredBanks.end())return;
            for(const auto& association:group.associations)
                if(!declared->second.count(association.second.name))return;
            groups.emplace(id,std::move(group));
        };
        // Unloaded LCS groups retain only their group ID and first animation
        // ID. Restore the reLCS definitions only when GAME.DTZ independently
        // declares every referenced clip in the same native bank.
        struct WeaponGroup {
            uint32_t id;
            const char* bank;
            std::initializer_list<const char*> names;
        };
        const WeaponGroup weapons[]={
            {9,"knife",{"weapon_knife_1","weapon_knife_2","knife_part","weapon_knifeidle","weapon_knife_3"}},
            {10,"baseball",{"weapon_bat_h","weapon_bat_v","bat_part","weapon_bat_h","weapon_golfclub"}},
            {11,"baseball",{"weapon_bat_h","weapon_golfclub","bat_part","weapon_bat_h","weapon_bat_v"}},
            {12,"chainsaw",{"weapon_csaw","weapon_csawlo","csaw_part"}},
            {13,"python",{"python_fire","python_crouchfire","python_reload","python_crouchreload"}},
            {14,"colt45",{"colt45_fire","colt45_crouchfire","colt45_reload","colt45_crouchreload","colt45_cop"}},
            {15,"shotgun",{"shotgun_fire","shotgun_crouchfire"}},
            {16,"buddy",{"buddy_fire","buddy_crouchfire","buddy_reload"}},
            {17,"tec",{"tec_fire","tec_crouchfire","tec_reload","tec_crouchreload"}},
            {18,"uzi",{"uzi_fire","uzi_crouchfire","uzi_reload","uzi_crouchreload"}},
            {19,"rifle",{"rifle_fire","rifle_crouchfire","rifle_load","rifle_crouchload"}},
            {20,"m60",{"m60_fire","m60_fire","m60_reload"}},
            {21,"sniper",{"sniper_fire","sniper_fire","sniper_reload"}},
            {22,"grenade",{"weapon_throw","weapon_throwu","weapon_start_throw"}},
            {23,"flame",{"flame_fire"}},
            {24,"rocketla",{"rocket_fire","rocket_fire","rocket_reload"}}
        };
        for(const auto& definition:weapons) {
            StorylandAnimationGroup group;
            group.name=group.bank=definition.bank;
            uint32_t animationId=201;
            for(const char* name:definition.names) {
                const uint32_t index=animationId-201;
                uint16_t flags=definition.id>=13&&index>=4?0:0x18;
                if(definition.id<=12) {
                    if(index==3)flags=0x2;
                    else if(definition.id!=10&&definition.id!=11) {
                        if(index==2)flags|=0x400;
                        else if(index==4)flags|=0x40;
                    }
                }
                group.associations.emplace(animationId++,StorylandAnimationAssociation{name,flags});
            }
            restoreGroup(definition.id,201,std::move(group));
        }
        struct GroupDefinition {
            uint32_t id,first;
            const char* bank;
            std::initializer_list<StorylandAnimationAssociation> associations;
        };
        const GroupDefinition streamed[]={
            {1,170,"van",{{"van_openl",0x18},{"van_getinl",0x18},{"van_closel",0x18},{"van_getoutl",0x18},
                {"van_open",0x18},{"van_getin",0x18},{"van_close",0x18},{"van_getout",0x18}}},
            {2,178,"coach",{{"coach_opnl",0x18},{"coach_opnr",0x18},{"coach_inl",0x18},
                {"coach_inr",0x18},{"coach_outl",0x18}}},
            {25,210,"medic",{{"cpr",0x18}}},
            {26,206,"sunbathe",{{"bather",0x12}}},
            {27,211,"playidles",{{"stretch",0x1C},{"time",0x1C},{"shldr",0x1C},{"strleg",0x1C}}},
            {28,215,"riot",{{"riot_angry",0x1A},{"riot_angry_b",0x1A},{"riot_chant",0x1A},
                {"riot_punches",0x1A},{"riot_shout",0x1A},{"riot_challenge",0x18},{"riot_fuku",0x18}}},
            {29,222,"strip",{{"strip_a",0x1A},{"strip_b",0x1A},{"strip_c",0x1A},
                {"strip_d",0x1A},{"strip_e",0x18},{"strip_f",0x1A},{"strip_g",0x18}}},
            {30,206,"lance",{{"lance",0x12}}},
            {53,0,"skate",{{"skate_run",0x162},{"skate_sprint",0x162},
                {"skate_sprint",0x162},{"skate_idle",0x2}}},
            {60,229,"mpnote",{{"mpnote",0x210},{"mpnoteloop",0x212}}},
            {61,231,"cs_misc",{{"idle_look",0x218},{"idle_no",0x218},{"idle_yes",0x218},
                {"idle_chat2",0x218},{"idle_cough",0x218},{"idle_giggle_female",0x218},
                {"idle_tough_chat",0x218},{"idle_cell_talk",0x218}}},
            {62,239,"donh2",{{"donh2_camera",0x218}}},
            {63,240,"donh3",{{"donh3_happy",0x214}}},
            {64,241,"jdt2",{{"jdt2_anxious_talk",0x218},{"jdt2_shrug",0x218}}},
            {65,243,"jdt4",{{"jdt4_dildo_talk",0x218}}},
            {66,244,"jdt5",{{"jdt5_calm_down",0x218},{"jdt5_point",0x218}}},
            {67,246,"jdt6",{{"jdt6_pickup",0x218},{"jdt6_doorknock",0x218}}},
            {68,248,"mar1",{{"mar1_skirt",0x218}}},
            {69,249,"mar2",{{"mar2_cell_answer",0x218},{"mar2_cell_end",0x218},
                {"mar2_cell_talk",0x218},{"mar2_foot_tap",0x218}}},
            {70,253,"mar3",{{"mar3_hoochy",0x218},{"mar3_note_idle",0x218},{"mar3_note_pick",0x218}}},
            {71,256,"sal1",{{"sal1_briefcase_down",0x218}}},
            {72,257,"sal2",{{"sal2_idle_seated",0x218},{"sal2_seat_to_stand",0x218}}},
            {73,259,"sal3",{{"sal3_seated_talk",0x218},{"sal3_seated_idle",0x218},{"sal3_sit_down",0x210}}},
            {74,262,"sal4",{{"sal4_dustdown",0x218},{"sal4_girlrun",0x218}}},
            {75,264,"sal6",{{"sal6_angry_seated",0x218},{"sal6_idle_seated",0x218}}},
            {76,266,"sal7",{{"sal7_lookout",0x218}}},
            {77,267,"vic2",{{"vic2_point_angry",0x218}}},
            {78,268,"vic3",{{"vic3_waft",0x218},{"vic3_pickup_roll",0x218}}},
            {79,270,"vic4",{{"vic4_carry_box",0x218},{"vic4_cell_look",0x218},{"vic4_crate_idle",0x218}}},
            {80,273,"vic6",{{"vic6_cell_angry",0x218}}},
            {81,274,"tourist",{{"tourist3",0x218},{"tourist2",0x218},{"tourist1",0x218}}},
            {82,277,"mac2",{{"mac2_plead",0x218}}},
            {83,278,"vic7",{{"vic7_prod_with_foot",0x218}}}
        };
        for(const auto& definition:streamed) {
            StorylandAnimationGroup group;
            group.name=group.bank=definition.bank;
            uint32_t id=definition.first;
            for(const auto& association:definition.associations)group.associations.emplace(id++,association);
            restoreGroup(definition.id,definition.first,std::move(group));
        }
        const StorylandAnimationAssociation rider[]={
            {"ride",0x4},{"still",0x2014},{"left",0x2010},{"right",0x2010},
            {"back",0x2010},{"fwd",0x2010},{"pushes",0x2016},{"jumponr",0x18},
            {"jumponl",0x18},{"kick",0x18},{"hit",0x18},{"getoffrhs",0x18},
            {"getofflhs",0x18},{"getoffback",0x58},{"drivebylhs",0x2014},
            {"drivebyrhs",0x2014},{"drivebyft",0x2014},{"passenger",0x2004}
        };
        const char* riderBanks[]={"bikes","bikev","bikeh","biked"};
        for(uint32_t index=0;index<4;++index) {
            StorylandAnimationGroup group;
            group.name=group.bank=riderBanks[index];
            uint32_t id=183;
            for(const auto& association:rider)
                group.associations.emplace(id++,StorylandAnimationAssociation{
                    group.bank+"_"+association.name,association.flags});
            restoreGroup(3+index,183,std::move(group));
        }
        return groups.count(0)!=0;
    }
    const StorylandAnimationAssociation* find(uint32_t group,uint32_t id) const {
        const auto found=groups.find(group);
        if(found==groups.end())return nullptr;
        const auto association=found->second.associations.find(id);
        return association==found->second.associations.end()?nullptr:&association->second;
    }
    uint32_t resolve(uint32_t group,uint32_t id,const std::vector<StorylandAnimClip>& clips) const {
        const auto found=groups.find(group);
        if(found==groups.end())return UINT32_MAX;
        const auto association=found->second.associations.find(id);
        if(association==found->second.associations.end())return UINT32_MAX;
        for(const auto& clip:clips)
            if(matchesKey(clip.bankName,found->second.bank)&&matchesKey(clip.name,association->second.name))return clip.index;
        return UINT32_MAX;
    }
};
