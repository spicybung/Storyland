#include "src/storyland/storyland_play_test.h"
#include "src/storyland/storyland_retail_melee.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_weapon_animation_policy.h"
#include "src/leeds/leeds_model_materials.h"
#include "src/storyland/storyland_vehicle_suspension.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_exit.h"
#include "src/re/reVCS/vehicles/storyland_vcs_boat_entry.h"
#include "src/storyland/storyland_mobile_animation.h"
#include "src/leeds/leeds_native_texture.h"
#include "src/re/reLCS2/runtime/storyland_lcs_population.h"
#include "src/storyland/storyland_game_files.h"
#include "src/leeds/leeds_native_layout.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <exception>
#include <iterator>
#include <new>
#include <set>
#include <map>
#include <filesystem>
#include <cwctype>
#include "src/storyland/storyland_scm.h"

namespace {
bool fatalPs2PacketError(const std::string& error) {
    return error.rfind("PS2 DMA/VIF/VU/FIFO [", 0u) == 0u;
}
std::string clipLower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
    return value;
}
bool clipMatches(std::string_view value,std::string_view normalized) {
    return StorylandAnimationGroups::matchesKey(value,normalized);
}
bool decodedBankHasClips(StorylandAnimFile& decoded,const std::string& bank,
    const std::vector<std::string>& required,std::string& error) {
    for(const auto& name:required) {
        const auto normalized=clipLower(name);
        const auto clip=std::find_if(decoded.clips().begin(),decoded.clips().end(),[&](const auto& value) {
            return clipMatches(value.name,normalized);
        });
        if(clip==decoded.clips().end()||!decoded.setActiveClipIndex(clip->index)||!decoded.hasDecodedMotion()) {
            error="Animation bank "+bank+" lacks decoded native clip "+name;
            return false;
        }
        if(std::any_of(std::next(clip),decoded.clips().end(),[&](const auto& value) {
            return clipMatches(value.name,normalized);
        })) {
            error="Animation bank "+bank+" contains ambiguous native clip "+name;
            return false;
        }
    }
    return true;
}
std::string clipKey(const std::string& value) {
    std::string result;
    for(unsigned char ch:value)if(std::isalnum(ch))result.push_back(char(std::tolower(ch)));
    return result;
}
bool clipKeyMatches(std::string_view value,std::string_view normalized) {
    size_t index=0;
    for(unsigned char ch:value)if(std::isalnum(ch)) {
        if(index==normalized.size()||char(std::tolower(ch))!=normalized[index])return false;
        ++index;
    }
    return index==normalized.size();
}
bool isDeathClip(const std::string& name) {
    const auto key=clipKey(name);
    return key.find("koshot")!=std::string::npos || key.find("fallcollapse")!=std::string::npos ||
        key.find("death")!=std::string::npos || key.find("floorhit")!=std::string::npos ||
        key.find("dief")!=std::string::npos || key.find("dieback")!=std::string::npos;
}
uint32_t vehicleAssociationGroup(const std::string& action,const StorylandVehicleAnimationOptions& options) {
    using Variant=StorylandVehicleEntryVariant;
    if(options.vcsJetSki)return action=="align"?0u:StorylandVcsJetSkiAnimationGroup;
    if(options.quad&&action=="exit"&&options.variant==Variant::CrawlOut)return 0;
    if(options.twoWheels||options.quad) {
        if((action=="open"&&(options.variant==Variant::BikePickup||options.variant==Variant::BikePullup))||
            (action=="hijack"&&options.variant!=Variant::BikeFront))return 0;
        return uint32_t(options.animationGroup);
    }
    if(action=="exit"&&options.vcsHelicopter&&options.variant==Variant::MovingExit)
        return StorylandVcsHelicopterBailoutGroup;
    if(action=="exit"&&(options.variant==Variant::CrawlOut||options.variant==Variant::MovingExit))return 0;
    if(options.bus&&(action=="open"||action=="enter"||action=="exit"))return 2;
    if(options.van&&options.rearDoor&&
        (action=="open"||action=="enter"||action=="exit"||action=="close_inside"))return 1;
    return 0;
}
uint32_t vehicleAssociation(const std::string& action,const StorylandVehicleAnimationOptions& options,
    const StorylandAnimationGroups& groups) {
    using Variant=StorylandVehicleEntryVariant;
    if(options.vcsOrdinaryBoat)return action=="sit"?(options.boat?110u:97u):UINT32_MAX;
    if(options.vcsJetSki)return storylandVcsJetSkiAssociation(action,options.rightHandSide,
        options.passenger,options.strongWaterContact,options.highAlign);
    if(options.quad&&action=="exit"&&options.variant==Variant::CrawlOut)return options.rightHandSide?128:129;
    if(options.twoWheels||options.quad) {
        if(action=="open"&&options.variant==Variant::BikePickup)return options.rightHandSide?114:115;
        if(action=="open"&&options.variant==Variant::BikePullup)return options.rightHandSide?116:117;
        const auto group=groups.groups.find(uint32_t(options.animationGroup));
        if(action=="bike_victim_hit"&&groups.vcsLayout&&group!=groups.groups.end())
            return 197;
        if(action=="hijack"&&options.variant!=Variant::BikeFront) {
            const bool motorcycle=group!=groups.groups.end()&&
                (group->second.bank=="bikes"||group->second.bank=="bikev"||
                    group->second.bank=="bikeh"||group->second.bank=="biked");
            if(groups.vcsLayout&&motorcycle)return options.rightHandSide?119:118;
            return options.rightHandSide?118:119;
        }
        if(group==groups.groups.end()||group->second.associations.empty())return UINT32_MAX;
        const uint32_t first=group->second.associations.begin()->first;
        if(action=="sit")return first+(options.passenger?17u:0u);
        if((action=="enter"||action=="hijack")&&options.variant==Variant::BikeFront)return first+9u;
        if(action=="enter"||action=="exit") {
            const auto& bank=group->second.bank;
            if(bank=="bikes"||bank=="bikev"||bank=="bikeh"||bank=="biked") {
                // Native LHS associations use the clips named jumponR/getoffRHS.
                // The association side, not the filename, selects the door.
                if(action=="enter")return first+(options.rightHandSide?8u:7u);
                return first+(options.rearExit||options.variant==Variant::MovingExit?13u:
                    options.rightHandSide?12u:11u);
            }
            const std::string suffix=action=="enter"?(options.rightHandSide?"jumponr":"jumponl"):
                options.rearExit||options.variant==Variant::MovingExit?"getoffback":
                options.rightHandSide?"getoffrhs":"getofflhs";
            for(const auto& association:group->second.associations) {
                const auto name=clipKey(association.second.name);
                if(name.size()>=suffix.size()&&name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0)
                    return association.first;
            }
            return action=="enter"?first+(options.rightHandSide?7u:8u):
                first+(options.rearExit||options.variant==Variant::MovingExit?13u:options.rightHandSide?11u:12u);
        }
        return UINT32_MAX;
    }
    if(action=="exit"&&options.variant==Variant::CrawlOut)return options.rightHandSide?128:129;
    if(action=="exit"&&options.variant==Variant::MovingExit)return options.vcsHelicopter?
        StorylandVcsHelicopterBailoutAnimation:options.rightHandSide?131:130;
    if(action=="shuffle")return options.low?96:95;
    if(action=="enter"&&options.jumpIn)return options.rightHandSide?79:78;
    if(options.bus) {
        const uint32_t first=groups.vcsLayout?182u:178u;
        if(action=="open")return first+(options.rightHandSide?1u:0u);
        if(action=="enter")return first+(options.rightHandSide?3u:2u);
        if(action=="exit")return first+4u;
    }
    if(options.van&&options.rearDoor) {
        const uint32_t first=(groups.vcsLayout?174u:170u)+(options.rightHandSide?4u:0u);
        if(action=="open")return first;
        if(action=="enter")return first+1;
        if(action=="close_inside")return first+2;
        if(action=="exit")return first+3;
    }
    if(action=="sit")return options.boat?110:options.passenger?(options.low?100:99):(options.low?98:97);
    return storylandVehicleAssociation(action,options.low,options.rightHandSide,groups.vcsLayout);
}
}

bool storylandNativeModelAvailable(const StorylandDtzArchive& archive, int32_t model) {
    const auto& bytes = archive.unpackedBytes();
    const auto word = [&](size_t offset) {
        uint32_t value = 0;
        if (offset <= bytes.size() && bytes.size() - offset >= sizeof(value)) {
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
        }
        return value;
    };
    if (model < 0 || uint32_t(model) >= word(0x38)) {
        return false;
    }
    const uint32_t table = word(0x3C);
    const uint32_t record = table ? word(size_t(table) + size_t(model) * 4) : 0;
    return record >= 32 && uint64_t(record) + 0x24 <= bytes.size();
}

bool storylandValidateSpecialModelAsset(const StorylandDtzArchive& archive, int32_t model,
    const std::string& name, std::string& error) {
    size_t entry = 0;
    const std::wstring stem(name.begin(), name.end());
    if (!storylandNativeModelAvailable(archive, model) ||
        !archive.findDirEntryByStemAndExtension(stem, {L".mdl", L".dff"}, entry)) {
        error = "Special model needs an existing target IDE record and a named IMG model.";
        return false;
    }
    if (archive.dirEntries()[entry].sectorCount > 4096) {
        error = "Special model exceeds the native asset decoder limit.";
        return false;
    }
    std::vector<uint8_t> payload;
    StorylandModelFile decoded;
    if (!archive.extractDirEntryBytes(entry, payload, error) ||
        !decoded.loadFromMemory(payload, stem + L".mdl", error)) {
        return false;
    }
    const auto& bytes = archive.unpackedBytes();
    uint32_t table = 0;
    uint32_t targetInfo = 0;
    std::memcpy(&table, bytes.data() + 0x3C, sizeof(table));
    std::memcpy(&targetInfo, bytes.data() + table + size_t(model) * 4, sizeof(targetInfo));
    const uint8_t type = bytes[targetInfo + 0x10];
    const bool compatible = (type == 7 && decoded.modelKind() == StorylandModelKind::PedModel) ||
        ((type == 1 || type == 3) && decoded.modelKind() == StorylandModelKind::SimpleModel);
    if (!compatible) {
        error = "Special model payload must match a native ped or simple-model target slot.";
        return false;
    }
    // Native 0x3C81B0 replaces the stream name and TXD association. The target
    // model's physical and collision records remain attached to that slot.
    return true;
}

bool storylandPendingModelNeedsAnimation(const StorylandDtzArchive& archive,
    const std::set<int32_t>& requested, const std::set<int32_t>& resident, const std::string& bank) {
    const auto& bytes = archive.unpackedBytes();
    const auto word = [&](size_t offset) {
        uint32_t value = 0;
        if (offset <= bytes.size() && bytes.size() - offset >= sizeof(value)) {
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
        }
        return value;
    };
    StorylandNativeAnimationLayout layout;
    if (!storylandNativeAnimationLayout(bytes, archive.gameVariant() == StorylandDtzGameVariant::Vcs, layout)) {
        return false;
    }
    const uint32_t blocks = layout.blocks;
    const uint32_t count = layout.blockCount;
    int32_t bankIndex = -1;
    for (uint32_t index = 0; index < count; ++index) {
        const size_t row = size_t(blocks) + size_t(index) * layout.blockStride;
        std::string name;
        for (size_t letter = 0; letter < 20 && bytes[row + letter]; ++letter) {
            name.push_back(char(bytes[row + letter]));
        }
        if (clipLower(name) == clipLower(bank)) {
            bankIndex = int32_t(index);
            break;
        }
    }
    if (bankIndex < 0) {
        return false;
    }
    const uint32_t table = word(0x3C);
    for (int32_t model : requested) {
        if (resident.count(model) || !storylandNativeModelAvailable(archive, model)) {
            continue;
        }
        const uint32_t info = word(size_t(table) + size_t(model) * 4);
        size_t field = 0;
        switch (bytes[size_t(info) + 0x10]) {
        case 2:
        case 5:
        case 7:
        case 8:
            field = 0x2C;
            break;
        case 4:
            field = 0x40;
            break;
        case 6:
            field = 0x26C;
            break;
        default:
            continue;
        }
        if (uint64_t(info) + field + 4 <= bytes.size() && word(size_t(info) + field) == uint32_t(bankIndex)) {
            return true;
        }
    }
    return false;
}

bool StorylandPlayTestAnimation::load(const StorylandDtzArchive& archive,bool player,std::string& error) {
    if(!loadFromDtzBytes(archive.unpackedBytes(),archive.gameVariant()==StorylandDtzGameVariant::Vcs,player,error))return false;
    std::set<std::string> banks;
    for(uint32_t group=1;group<=24;++group) {
        const auto found=groups.groups.find(group);
        if(found!=groups.groups.end()&&found->second.bank!="ped")banks.insert(found->second.bank);
    }
    if(groups.vcsLayout) {
        const auto fight=groups.groups.find(63);
        if(fight!=groups.groups.end()&&fight->second.bank!="ped")banks.insert(fight->second.bank);
        if(player) {
            const auto driveBy=groups.groups.find(59);
            if(driveBy!=groups.groups.end()&&driveBy->second.bank!="ped")banks.insert(driveBy->second.bank);
        }
    }
    size_t loaded=0,recovered=0,totalBytes=0;
    std::string missing;
    for(const auto& bank:banks) {
        const std::wstring stem(bank.begin(),bank.end());
        size_t entry=0;std::vector<uint8_t> bytes;std::string message;
        bool ready=false;
        if(archive.findDirEntryByStemAndExtension(stem,{L".anim",L".ifp"},entry)&&
            archive.dirEntries()[entry].sectorCount<=4096&&
            archive.extractDirEntryBytes(entry,bytes,message)) {
            if(totalBytes+bytes.size()<=32u*1024u*1024u)ready=importWeaponBank(bytes,bank,message);
        }
        if(!ready) {
            std::vector<std::string> names;
            for(const auto& group:groups.groups)if(group.second.bank==bank)
                for(const auto& association:group.second.associations)names.push_back(association.second.name);
            const auto nativeBank=bankClips.find(bank);
            if(nativeBank!=bankClips.end())
                names.insert(names.end(),nativeBank->second.begin(),nativeBank->second.end());
            std::sort(names.begin(),names.end());names.erase(std::unique(names.begin(),names.end()),names.end());
            if(archive.extractNativeAnimationBank(names,bytes,message)&&
                totalBytes+bytes.size()<=32u*1024u*1024u&&importWeaponBank(bytes,bank,message)) {
                ready=true;++recovered;
            }
        }
        if(ready) {++loaded;totalBytes+=bytes.size();}
        else {if(!missing.empty())missing+=", ";missing+=bank;}
    }
    bankDiagnostic=std::to_string(loaded)+" decoded weapon banks; "+std::to_string(recovered)+" recovered by native clip names";
    if(!missing.empty())bankDiagnostic+="; unavailable: "+missing;
    // Rider banks are not part of the resident pedestrian bank. Load their
    // authored mount/ride/dismount clips once, before a vehicle can be entered.
    for(const auto& entry:groups.groups) {
        const auto& bank=entry.second.bank;
        const bool jetSki=groups.vcsLayout&&entry.first==StorylandVcsJetSkiAnimationGroup;
        if(!jetSki&&bank!="bikes"&&bank!="bikev"&&bank!="bikeh"&&bank!="biked"&&bank!="bmx"&&bank!="quad")continue;
        std::string message;
        if(!loadAnimationGroup(archive,int32_t(entry.first),message))
            bankDiagnostic+="; rider bank "+bank+": "+message;
    }
    return true;
}

bool StorylandPlayTestAnimation::hasAnimationGroup(int32_t id) const {
    const auto group=groups.groups.find(uint32_t(id));
    if(id<0||group==groups.groups.end()||group->second.associations.empty())return false;
    for(const auto& association:group->second.associations) {
        const auto found=std::find_if(animation.clips().begin(),animation.clips().end(),[&](const auto& clip) {
            return clipMatches(clip.bankName,group->second.bank)&&clipMatches(clip.name,association.second.name);
        });
        if(found==animation.clips().end())return false;
    }
    return true;
}

std::string StorylandPlayTestAnimation::animationGroupBank(int32_t id) const {
    const auto found = groups.groups.find(uint32_t(id));
    return found == groups.groups.end() ? std::string{} : found->second.bank;
}

bool StorylandPlayTestAnimation::knowsAnimationBank(const std::string& bank) const {
    return bankClips.count(clipLower(bank)) != 0;
}

bool StorylandPlayTestAnimation::hasAnimationBank(const std::string& bank) const {
    const auto name = clipLower(bank);
    const auto found = bankClips.find(name);
    if (found == bankClips.end() || found->second.empty()) {
        return false;
    }
    for (const auto& required : found->second) {
        const auto clip = std::find_if(animation.clips().begin(), animation.clips().end(), [&](const auto& value) {
            return clipMatches(value.bankName,name) && clipMatches(value.name,required);
        });
        if (clip == animation.clips().end()) {
            return false;
        }
    }
    return true;
}

bool StorylandPlayTestAnimation::loadAnimationBank(const StorylandDtzArchive& archive,
    const std::string& bank, std::string& error) {
    const auto name = clipLower(bank);
    if (hasAnimationBank(name)) {
        error.clear();
        return true;
    }
    const auto found = bankClips.find(name);
    if (found == bankClips.end() || found->second.empty()) {
        error = "The native animation table has no clips for bank " + name;
        return false;
    }
    const std::wstring stem(name.begin(), name.end());
    size_t entry = 0;
    std::vector<uint8_t> bytes;
    const bool extracted = archive.findDirEntryByStemAndExtension(stem, {L".anim", L".ifp"}, entry) &&
        archive.dirEntries()[entry].sectorCount <= 4096 && archive.extractDirEntryBytes(entry, bytes, error);
    const auto importBank = [&]() {
        if (bytes.size() > 8u * 1024u * 1024u) {
            error = "Animation bank exceeds the decoding limit.";
            return false;
        }
        StorylandAnimFile decoded;
        if (!decoded.loadNativeBank(bytes, stem + L".anim", groups.vcsLayout, error) ||
            !decodedBankHasClips(decoded, name, found->second, error)) {
            return false;
        }
        animation.appendDecodedBank(decoded, name, found->second);
        secondaryAnimation.appendDecodedBank(decoded, name, found->second);
        upperAnimation.appendDecodedBank(decoded, name, found->second);
        if (!hasAnimationBank(name)) {
            error = "Decoded animation bank is missing native clips: " + name;
            return false;
        }
        error.clear();
        return true;
    };
    if (extracted && importBank()) {
        return true;
    }
    // Recovered directory labels are only candidates. A readable payload
    // with different clips must still fall back to the native bank names.
    if (!archive.extractNativeAnimationBank(found->second, bytes, error)) {
        return false;
    }
    return importBank();
}

void StorylandPlayTestAnimation::removeAnimationBank(const std::string& bank) {
    locomotionTracks.clear();
    const auto name = clipLower(bank);
    // Explicit REMOVE_ANIMATION reaches the native unload path directly.
    // Unlike automatic reference release, it also invalidates live trees.
    animation.removeDecodedBank(name);
    secondaryAnimation.removeDecodedBank(name);
    upperAnimation.removeDecodedBank(name);
}

bool StorylandPlayTestAnimation::loadAnimationGroup(const StorylandDtzArchive& archive,int32_t id,std::string& error) {
    if(hasAnimationGroup(id)) {error.clear();return true;}
    const auto group=groups.groups.find(uint32_t(id));
    if(id<0||group==groups.groups.end()) {error="Unknown animation group "+std::to_string(id);return false;}
    const auto& bank=group->second.bank;
    std::vector<std::string> names;
    for(const auto& row:groups.groups)if(row.second.bank==bank)
        for(const auto& association:row.second.associations)names.push_back(association.second.name);
    // The streaming check refers to the complete native bank, including
    // clips that are not selected by an association group (for example,
    // motorcycle hijacks). Import them with the group so a ready bank is
    // also recognized by subsequent model-animation streaming requests.
    const auto nativeBank=bankClips.find(clipLower(bank));
    if(nativeBank!=bankClips.end())
        names.insert(names.end(),nativeBank->second.begin(),nativeBank->second.end());
    std::sort(names.begin(),names.end());names.erase(std::unique(names.begin(),names.end()),names.end());
    std::vector<uint8_t> bytes;size_t entry=0;
    const std::wstring stem(bank.begin(),bank.end());
    const bool extracted=archive.findDirEntryByStemAndExtension(stem,{L".anim",L".ifp"},entry)&&
        archive.dirEntries()[entry].sectorCount<=4096&&archive.extractDirEntryBytes(entry,bytes,error);
    const auto importBank=[&]() {
        if(bytes.size()>8u*1024u*1024u) {error="Animation bank exceeds the streaming limit.";return false;}
        StorylandAnimFile decoded;
        if(!decoded.loadNativeBank(bytes,stem+L".anim",groups.vcsLayout,error)||
            !decodedBankHasClips(decoded,bank,names,error))return false;
        animation.appendDecodedBank(decoded,bank,names);
        secondaryAnimation.appendDecodedBank(decoded,bank,names);
        upperAnimation.appendDecodedBank(decoded,bank,names);
        if(!hasAnimationGroup(id)) {error="Animation bank "+bank+" is missing required associations.";return false;}
        error.clear();return true;
    };
    if(extracted&&importBank())return true;
    // A recovered directory name can point at a readable bank containing
    // different clips. Verify its associations before accepting that entry;
    // the native clip names can still resolve the correct IMG payload.
    if(!archive.extractNativeAnimationBank(names,bytes,error))return false;
    return importBank();
}

bool StorylandPlayTestAnimation::importWeaponBank(const std::vector<uint8_t>& bytes,
    const std::string& bank,std::string& error) {
    if(bytes.size()<32||bytes.size()>8u*1024u*1024u) {error="Weapon bank size is invalid.";return false;}
    if(std::string(bytes.begin(),bytes.begin()+4)!="mina"&&std::string(bytes.begin(),bytes.begin()+4)!="ANPK") {
        error="Weapon bank does not begin with a native animation header.";return false;
    }
    std::vector<std::string> allowed;
    for(const auto& group:groups.groups)if(group.second.bank==clipLower(bank))
        for(const auto& association:group.second.associations)allowed.push_back(association.second.name);
    if(allowed.empty()) {error="The retail association table does not reference this bank.";return false;}
    const auto nativeBank=bankClips.find(clipLower(bank));
    if(nativeBank!=bankClips.end())
        allowed.insert(allowed.end(),nativeBank->second.begin(),nativeBank->second.end());
    std::sort(allowed.begin(),allowed.end());
    allowed.erase(std::unique(allowed.begin(),allowed.end()),allowed.end());
    StorylandAnimFile decoded;
    if(!decoded.loadNativeBank(bytes,std::wstring(bank.begin(),bank.end())+L".anim",groups.vcsLayout,error)||
        !decodedBankHasClips(decoded,bank,allowed,error))return false;
    const bool upperAdded=upperAnimation.appendDecodedBank(decoded,clipLower(bank),allowed);
    const bool fullAdded=animation.appendDecodedBank(decoded,clipLower(bank),allowed);
    const bool secondaryAdded=secondaryAnimation.appendDecodedBank(decoded,clipLower(bank),allowed);
    if(!upperAdded&&!fullAdded&&!secondaryAdded&&!hasNamedAnimationBank(bank,allowed)) {
        error="Weapon bank has no new decoded clips matching its retail associations.";return false;
    }
    error.clear();
    return true;
}

bool StorylandPlayTestAnimation::hasNamedAnimationBank(const std::string& bank,
    const std::vector<std::string>& clips) const {
    if(clips.empty())return false;
    const auto requestedBank=clipLower(bank);
    return std::all_of(clips.begin(),clips.end(),[&](const auto& name) {
        const auto requestedName=clipLower(name);
        return std::any_of(animation.clips().begin(),animation.clips().end(),[&](const auto& clip) {
            return clipMatches(clip.bankName,requestedBank)&&clipMatches(clip.name,requestedName);
        });
    });
}

bool StorylandPlayTestAnimation::loadNamedAnimationBank(const StorylandDtzArchive& archive,
    const std::string& bank,const std::vector<std::string>& clips,std::string& error) {
    if(hasNamedAnimationBank(bank,clips)) {error.clear();return true;}
    if(bank.empty()||clips.empty()) {error="Named animation bank has no verified clip definitions.";return false;}
    const auto name=clipLower(bank);
    const std::wstring stem(name.begin(),name.end());
    std::vector<uint8_t> bytes;size_t entry=0;
    const bool extracted=archive.findDirEntryByStemAndExtension(stem,{L".anim",L".ifp"},entry)&&
        archive.dirEntries()[entry].sectorCount<=4096&&archive.extractDirEntryBytes(entry,bytes,error);
    if(extracted&&importNamedAnimationBank(bytes,name,clips,error))return true;
    if(!archive.extractNativeAnimationBank(clips,bytes,error))return false;
    return importNamedAnimationBank(bytes,name,clips,error);
}

bool StorylandPlayTestAnimation::importNamedAnimationBank(const std::vector<uint8_t>& bytes,
    const std::string& bank,const std::vector<std::string>& clips,std::string& error) {
    if(hasNamedAnimationBank(bank,clips)) {error.clear();return true;}
    if(bank.empty()||clips.empty()||bytes.size()<32||bytes.size()>8u*1024u*1024u) {
        error="Named animation bank has invalid definitions or exceeds the streaming limit.";return false;
    }
    const auto name=clipLower(bank);
    const std::wstring stem(name.begin(),name.end());
    StorylandAnimFile decoded;
    if(!decoded.loadNativeBank(bytes,stem+L".anim",groups.vcsLayout,error)||
        !decodedBankHasClips(decoded,name,clips,error))return false;
    animation.appendDecodedBank(decoded,name,clips);
    secondaryAnimation.appendDecodedBank(decoded,name,clips);
    upperAnimation.appendDecodedBank(decoded,name,clips);
    if(!hasNamedAnimationBank(name,clips)) {error="Animation bank "+name+" lacks a required decoded wait-state clip.";return false;}
    error.clear();
    return true;
}

void StorylandPlayTestAnimation::unloadNamedAnimationBank(const std::string& bank) {
    locomotionTracks.clear();
    animation.removeDecodedBank(bank);secondaryAnimation.removeDecodedBank(bank);upperAnimation.removeDecodedBank(bank);
}

float StorylandPlayTestAnimation::bankClipDuration(const std::string& bank,const std::string& name) const {
    const auto requestedBank=clipLower(bank),requestedName=clipLower(name);
    for(const auto& clip:animation.clips())if(clipMatches(clip.bankName,requestedBank)&&clipMatches(clip.name,requestedName))
        return std::max(0.0f,clip.duration);
    return 0;
}

bool StorylandPlayTestAnimation::sampleBankClip(const StorylandModelFile& model,const std::string& bank,
    const std::string& name,float seconds,bool repeat,std::vector<StorylandModelPoint>& points,std::string& error,
    std::vector<std::array<float,16>>* frames,bool partial) {
    if(frames)frames->clear();
    if(!std::isfinite(seconds)) {error="Wait-state animation time must be finite.";return false;}
    const auto requestedBank=clipLower(bank),requestedName=clipLower(name);
    const auto found=std::find_if(animation.clips().begin(),animation.clips().end(),[&](const auto& clip) {
        return clipMatches(clip.bankName,requestedBank)&&clipMatches(clip.name,requestedName);
    });
    if(found==animation.clips().end()||!animation.setActiveClipIndex(found->index)||!animation.hasDecodedMotion()) {
        error="Wait-state animation has no decoded motion: "+bank+"/"+name;return false;
    }
    const float duration=std::max(0.001f,animation.durationSeconds());
    const float elapsed=std::max(0.0f,seconds);
    const float time=repeat?std::fmod(elapsed,duration):
        std::min(elapsed,groups.vcsLayout?duration-0.001f:duration);
    std::vector<StorylandModelPoint> bones;
    if(partial) {
        const auto upper=found->index;
        if(idle==UINT32_MAX||!upperAnimation.setActiveClipIndex(upper)||!upperAnimation.hasDecodedMotion()||
            !animation.setActiveClipIndex(idle)||!animation.hasDecodedMotion()) {
            error="Partial wait animation requires decoded standing and upper-body clips.";return false;
        }
        const float idleTime=std::fmod(elapsed,std::max(0.001f,animation.durationSeconds()));
        if(!groups.vcsLayout)return storylandSampleLcsAnimationAssociations(model,
            {{&upperAnimation,time,1,true},{&animation,idleTime,1,false}},true,
            model.previewPoints(),points,bones,error,frames);
        return storylandSampleModelAnimationPartial(model,animation,idleTime,upperAnimation,time,
            true,model.previewPoints(),points,bones,error,frames);
    }
    return storylandSampleModelAnimationLayered(model,animation,time,animation,time,0,
        nullptr,0,0,true,model.previewPoints(),points,bones,error,frames);
}

bool StorylandPlayTestAnimation::loadFromDtzBytes(const std::vector<uint8_t>& bytes,
    bool vcs,bool player,std::string& error) {
    bankDiagnostic.clear();
    idle=walk=run=sprint=upperAction=UINT32_MAX;
    playerAnimation=player;locomotionGroup=-1;fullBodyAction=false;
    weaponUsesArmAim=weaponUsesMovingAim=weaponUsesRocketLocomotion=false;
    aimAction=UINT32_MAX;
    if(!groups.load(bytes,vcs)) {
        error=std::string(vcs?"VCS":"LCS")+" native animation association table is invalid.";return false;
    }
    bankClips.clear();
    vehicleAnimationGroups.clear();
    const auto word = [&](size_t offset) {
        uint32_t value = 0;
        if (offset <= bytes.size() && bytes.size() - offset >= sizeof(value)) {
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
        }
        return value;
    };
    const auto label = [&](size_t offset, size_t count) {
        std::string value;
        if (offset > bytes.size() || count > bytes.size() - offset) {
            return value;
        }
        for (size_t index = 0; index < count && bytes[offset + index]; ++index) {
            value.push_back(char(bytes[offset + index]));
        }
        return clipLower(value);
    };
    StorylandNativeAnimationLayout layout;
    if (!storylandNativeAnimationLayout(bytes, vcs, layout)) {
        error = "The native animation block directory is invalid.";
        return false;
    }
    const uint32_t trees = layout.trees, blocks = layout.blocks;
    const uint32_t treeCount = layout.treeCount, blockCount = layout.blockCount;
    const uint32_t treeStride = layout.treeStride, blockStride = layout.blockStride;
    if (treeCount <= 50000 && blockCount <= 4096 &&
        uint64_t(trees) + uint64_t(treeCount) * treeStride <= bytes.size() &&
        uint64_t(blocks) + uint64_t(blockCount) * blockStride <= bytes.size()) {
        for (uint32_t index = 0; index < blockCount; ++index) {
            const size_t row = size_t(blocks) + size_t(index) * blockStride;
            const auto name = label(row, 20);
            const uint32_t first = word(row + layout.firstTreeOffset);
            const uint32_t count = word(row + layout.treeCountOffset);
            if (name.empty() || uint64_t(first) + count > treeCount) {
                continue;
            }
            auto& clips = bankClips[name];
            for (uint32_t clip = first; clip < first + count; ++clip) {
                const auto text = label(size_t(trees) + size_t(clip) * treeStride + 4, 24);
                if (!text.empty()) {
                    clips.push_back(text);
                }
            }
        }
    }
    if(blockCount<=4096&&uint64_t(blocks)+uint64_t(blockCount)*blockStride<=bytes.size()) {
        const uint32_t models=word(0x3C),count=word(0x38);
        if(models>=32&&count<=50000&&uint64_t(models)+uint64_t(count)*4<=bytes.size())
            for(uint32_t id=0;id<count;++id) {
                const uint32_t info=word(size_t(models)+size_t(id)*4);
                const size_t required=vcs?0x270u:0x190u;
                if(info<32||uint64_t(info)+required>bytes.size()||bytes[size_t(info)+0x10]!=6)continue;
                const uint32_t type=word(size_t(info)+(vcs?0x54u:0x38u));
                const uint32_t bankIndex=word(size_t(info)+(vcs?0x26Cu:0x18Cu));
                if((vcs?(type!=6&&type!=8&&type!=9):type!=5)||bankIndex>=blockCount)continue;
                const auto bank=label(size_t(blocks)+size_t(bankIndex)*blockStride,20);
                for(const auto& entry:groups.groups)if(entry.second.bank==bank) {
                    vehicleAnimationGroups.emplace(id,entry.first);break;
                }
            }
    }
    deathAnimationAvailable=false;
    lastDeathClipSampled=false;
    locomotionTracks.clear();
    if (!animation.loadFromDtz(bytes,vcs,error)) return false;
    if (!secondaryAnimation.loadFromDtz(bytes,vcs,error)) return false;
    const std::string walkName=player?"walk_player":"walk_civi";
    const std::string runName=player?"run_player":"run_civi";
    for (const auto& clip:animation.clips()) {
        const auto name=clipLower(clip.name);
        if(!clipMatches(clip.bankName,"ped"))continue;
        if(isDeathClip(name))deathAnimationAvailable=true;
        if (name=="idle_stance" && idle==UINT32_MAX) idle=clip.index;
        if (name==walkName && walk==UINT32_MAX) walk=clip.index;
        if (name==runName && run==UINT32_MAX) run=clip.index;
    }
    if (idle==UINT32_MAX || walk==UINT32_MAX || run==UINT32_MAX) {
        error="The current game's loaded animation bank is missing idle/walk/run clips."; return false;
    }
    for (uint32_t clip:{idle,walk,run}) {
        if (!animation.setActiveClipIndex(clip) || !animation.hasDecodedMotion()) {
            error="A locomotion clip has no decoded motion."; return false;
        }
    }
    const auto clipDistance = [&](uint32_t clip, float fallbackDistance, float& duration) {
        if (!animation.setActiveClipIndex(clip)) return fallbackDistance;
        duration = std::max(0.05f, animation.durationSeconds());
        for (const auto& track : animation.tracks()) {
            if (track.boneId != 0 || !(track.channelFlags & 2u) || track.keys.size() < 2) continue;
            const auto& start = track.keys.front();
            const auto& end = track.keys.back();
            const float displacement = std::hypot(end.tx - start.tx, end.ty - start.ty);
            if (std::isfinite(displacement) && displacement > 0.25f && displacement < 12.0f)
                return displacement;
        }
        return fallbackDistance;
    };
    walkStride = clipDistance(walk, 1.25f, walkDuration);
    runStride = clipDistance(run, 2.4f, runDuration);
    std::string upperError;
    if(!upperAnimation.loadFromDtz(bytes,
        vcs,upperError)) upperAction=UINT32_MAX;
    else if(upperAction!=UINT32_MAX&&(!upperAnimation.setActiveClipIndex(upperAction)||
        !upperAnimation.hasDecodedMotion())) upperAction=UINT32_MAX;
    if(!selectLocomotionGroup(player?(vcs?30:31):0,error))return false;
    for(unsigned variant=0;variant<2;++variant) {
        runStopRootKeys[variant].clear();
        const uint32_t clip=groups.resolve(0,(vcs?8u:5u)+variant,animation.clips());
        if(clip==UINT32_MAX||!animation.setActiveClipIndex(clip)||!animation.hasDecodedMotion())continue;
        for(const auto& track:animation.tracks())if(track.boneId==0&&(track.channelFlags&2u)&&track.keys.size()>1) {
            runStopRootKeys[variant]=track.keys;break;
        }
    }
    return true;
}

void StorylandPlayTestAnimation::updateLocomotionMetrics() {
    const auto measure=[&](uint32_t clip,float fallback,float& duration) {
        if(clip==UINT32_MAX||!secondaryAnimation.setActiveClipIndex(clip))return fallback;
        duration=std::max(0.001f,secondaryAnimation.durationSeconds());
        for(const auto& track:secondaryAnimation.tracks()) {
            if(track.boneId!=0||!(track.channelFlags&2u)||track.keys.size()<2)continue;
            const auto& first=track.keys.front();const auto& last=track.keys.back();
            const float distance=std::hypot(last.tx-first.tx,last.ty-first.ty);
            if(std::isfinite(distance)&&distance>0&&distance<12)return distance;
        }
        return fallback;
    };
    walkStride=measure(walk,1.25f,walkDuration);
    runStride=measure(run,2.4f,runDuration);
    sprintStride=measure(sprint,runStride,sprintDuration);
}

bool StorylandPlayTestAnimation::selectLocomotionGroup(int32_t group,std::string& error) {
    const auto groupEntry=groups.groups.find(uint32_t(group));
    if(group<0||groupEntry==groups.groups.end()) {
        error="Unknown native locomotion animation group "+std::to_string(group)+".";return false;
    }
    const std::array<uint32_t,4> ids=groups.vcsLayout?
        std::array<uint32_t,4>{6,2,4,5}:std::array<uint32_t,4>{3,0,1,2};
    if(locomotionGroup==group) {
        const std::array<uint32_t,4> current{idle,walk,run,sprint};
        const auto& clips=animation.clips();
        bool unchanged=true;
        for(size_t index=0;index<ids.size();++index) {
            const auto association=groupEntry->second.associations.find(ids[index]);
            const uint32_t clip=current[index];
            if(association==groupEntry->second.associations.end()||clip>=clips.size()||
                clips[clip].index!=clip||
                !StorylandAnimationGroups::matchesKey(clips[clip].bankName,groupEntry->second.bank)||
                !StorylandAnimationGroups::matchesKey(clips[clip].name,association->second.name)) {
                unchanged=false;break;
            }
        }
        if(unchanged) {error.clear();return true;}
    }
    std::array<uint32_t,4> selected{};
    for(size_t index=0;index<ids.size();++index) {
        selected[index]=groups.resolve(uint32_t(group),ids[index],animation.clips());
        if(selected[index]==UINT32_MAX) {
            error="Native locomotion group "+std::to_string(group)+" lacks decoded movement association "+
                std::to_string(ids[index])+".";return false;
        }
    }
    if(locomotionGroup==group&&idle==selected[0]&&walk==selected[1]&&run==selected[2]&&sprint==selected[3]) {
        error.clear();return true;
    }
    // Validate all clips before replacing the previous actor's selection. The
    // bank object is shared, while blend timing remains in each actor's state.
    for(const auto clip:selected) {
        if(!secondaryAnimation.setActiveClipIndex(clip)||!secondaryAnimation.hasDecodedMotion()) {
            error="Native locomotion group "+std::to_string(group)+" contains an undecoded movement clip.";return false;
        }
        if(!locomotionTracks.count(clip))locomotionTracks.emplace(clip,secondaryAnimation.tracks());
    }
    idle=selected[0];walk=selected[1];run=selected[2];sprint=selected[3];
    updateLocomotionMetrics();
    locomotionGroup=group;error.clear();return true;
}

bool StorylandPlayTestAnimation::advanceLocomotionDistance(int32_t group,StorylandPlayTestMotion motion,
    float travelled,float seconds,StorylandAnimationBlendState& state,std::string& error) {
    if(!std::isfinite(travelled)||travelled<0||!std::isfinite(seconds)||seconds<0||
        !std::isfinite(state.locomotionPhase)||(seconds==0&&travelled>0)) {
        error="Locomotion requires a finite distance, elapsed time and animation phase.";return false;
    }
    const float measured=seconds>0?travelled/seconds:0;
    if(!std::isfinite(measured)) {error="Locomotion speed is not finite.";return false;}
    if(!selectLocomotionGroup(group,error))return false;
    (void)motion; // Cadence follows travelled distance through state changes.
    const float stride=locomotionStrideAtSpeed(measured);
    if(!std::isfinite(stride)||stride<=0) {error="Native locomotion has no usable root-motion stride.";return false;}
    double phase=std::fmod(double(state.locomotionPhase)+double(travelled)/stride,1.0);
    if(phase<0)phase+=1;
    state.locomotionPhase=float(phase);
    state.measuredSpeed=measured;
    state.distanceDrivenPhase=true;
    state.walkSpeed=locomotionSpeed(StorylandPlayTestMotion::Walk);
    state.runSpeed=locomotionSpeed(StorylandPlayTestMotion::Run);
    error.clear();return true;
}

bool StorylandPlayTestAnimation::selectWeaponAction(const StorylandWeaponDefinition& weapon,
    const std::string& phase,bool moving,uint32_t serial,int pedGroup,bool fighting,bool police) {
    upperAction=aimAction=UINT32_MAX;
    fullBodyAction=false;
    weaponUsesArmAim=weapon.fireType!=0&&(weapon.nativeFlags&0x80u)!=0;
    weaponUsesMovingAim=storylandWeaponSupportsAimedLocomotion(groups.vcsLayout,weapon.nativeId,
        weapon.fireType,weapon.nativeFlags);
    weaponUsesRocketLocomotion=weaponUsesMovingAim&&weapon.nativeId==(groups.vcsLayout?32:30);
    if(groups.groups.empty()||(phase!="aim"&&phase!="fire"&&phase!="reload"))return false;
    const uint32_t firstAttack=groups.vcsLayout?214:201;
    const int group=playerAnimation?
        storylandNativePlayerWeaponLocomotionGroup(groups.vcsLayout,weapon.nativeId):
        storylandNativePedWeaponLocomotionGroup(groups.vcsLayout,weapon.nativeId,pedGroup);
    std::string error;
    // Keep each actor's native forward group. Aimed movement selects the
    // authored backward and side-step associations when composing its gait.
    if(group!=locomotionGroup&&!selectLocomotionGroup(group,error))return false;
    const auto nativeIdle=groups.resolve(uint32_t(locomotionGroup),groups.vcsLayout?6:3,animation.clips());
    if(nativeIdle!=UINT32_MAX)idle=nativeIdle;
    if(weapon.fireType==0&&fighting) {
        for(const auto& clip:animation.clips())
            if(clipMatches(clip.bankName,"ped")&&clipMatches(clip.name,"fightidle")) {idle=clip.index;break;}
    }
    if(weapon.fireType==0&&phase=="aim")return false;
    uint32_t selected=UINT32_MAX;
    const StorylandAnimationAssociation* selectedAssociation=nullptr;
    const bool policeThirdAction=!playerAnimation&&police&&phase!="reload"&&
        (groups.vcsLayout?weapon.animationGroup==14:(weapon.nativeFlags&0x40000u)!=0);
    if(weapon.nativeId<=0&&weapon.fireType==0&&phase=="fire") {
        const uint32_t moves[]={groups.vcsLayout?307u:201u,groups.vcsLayout?306u:202u,groups.vcsLayout?305u:203u};
        const uint32_t move=moving?(groups.vcsLayout?319:203):moves[(serial?serial-1:0)%3];
        selected=groups.resolve(groups.vcsLayout?63:7,move,upperAnimation.clips());
        selectedAssociation=groups.find(groups.vcsLayout?63:7,move);
    } else if(weapon.animationGroup>=0) {
        // The native melee groups contain PART (impact) and IDLE clips
        // alongside the real swings. Match the selected swing to the current
        // attack sequence; firearms retain their original fire association.
        const uint32_t aimId=weapon.animationGroup==22?firstAttack+2:
            policeThirdAction?firstAttack+4:firstAttack;
        if (weapon.fireType != 0)
            aimAction=groups.resolve(uint32_t(weapon.animationGroup),aimId,upperAnimation.clips());
        uint32_t id=phase=="reload"?firstAttack+2:phase=="aim"?aimId:firstAttack;
        if (weapon.fireType == 0 && phase == "fire")
            id=storylandRetailMeleeSwingAssociation(groups,weapon.animationGroup,serial,moving);
        else if(policeThirdAction)id=firstAttack+4;
        selected=groups.resolve(uint32_t(weapon.animationGroup),id,upperAnimation.clips());
        selectedAssociation=groups.find(uint32_t(weapon.animationGroup),id);
    }
    const float nativeLoopStart=!groups.vcsLayout&&policeThirdAction?11.0f/30.0f:weapon.animationLoopStart;
    // The hold clip may differ from the active reload/throw clip.
    aimPoseTime=0;
    if(aimAction<upperAnimation.clips().size()) {
        const float aimDuration=upperAnimation.clips()[aimAction].duration;
        aimPoseTime=weapon.animationGroup==22?aimDuration:std::clamp(nativeLoopStart,0.0f,aimDuration);
    }
    if(selected==UINT32_MAX||!selectedAssociation||!upperAnimation.setActiveClipIndex(selected)||
        !upperAnimation.hasDecodedMotion())return false;
    fullBodyAction=(selectedAssociation->flags&0x10u)==0;
    // For moving officers, the native fire/aim upper-body pose must not
    // replace the locomotion layer. Otherwise the world position advances
    // while the feet remain frozen in the gun-raise association.
    if(police&&moving&&weapon.fireType!=0)fullBodyAction=false;
    upperAction=selected;
    const float duration=std::max(0.001f,upperAnimation.durationSeconds());
    // upperStartedAt is the consumed shot event, which corresponds to the
    // native fire frame, not the earlier aim/loop-start frame.
    firePoseStart=weapon.fireType?std::clamp(std::max(nativeLoopStart,weapon.animationFireTime),0.0f,duration):0;
    firePoseEnd=weapon.fireType&&weapon.animationLoopEnd>firePoseStart?
        std::min(weapon.animationLoopEnd,duration):duration;
    reloadDuration=std::max(0.1f,weapon.reloadSeconds);
    return true;
}

void StorylandPlayTestAnimation::configureCombatDefinitions(std::vector<StorylandWeaponDefinition>& definitions,
    const StorylandModelFile* model) {
    const uint32_t moves[]={groups.vcsLayout?307u:201u,groups.vcsLayout?306u:202u,groups.vcsLayout?305u:203u};
    std::map<std::pair<uint32_t,uint32_t>,float> contacts;
    const auto contact=[&](uint32_t clip,uint32_t handId,float duration)->float {
        const auto key=std::make_pair(clip,handId);
        const auto previous=contacts.find(key);
        if(previous!=contacts.end())return previous->second;
        if(!model||!upperAnimation.setActiveClipIndex(clip)||!upperAnimation.hasDecodedMotion())return -1;
        size_t hand=SIZE_MAX,chest=SIZE_MAX;
        const auto& skeleton=model->armatureBones();
        for(size_t index=0;index<skeleton.size();++index) {
            if(skeleton[index].boneId==handId)hand=index;
            if(skeleton[index].boneId==3)chest=index;
        }
        if(hand==SIZE_MAX||chest==SIZE_MAX)return -1;
        const float forward=storylandPedModelForwardAngle(*model)*0.0174532925199433f;
        float best=-1e20f,time=-1;
        std::vector<StorylandModelPoint> points,bones;
        std::vector<std::array<float,16>> frames;
        std::string error;
        const int samples=std::clamp(int(std::ceil(duration*30)),3,150);
        for(int sample=1;sample<samples;++sample) {
            const float elapsed=duration*float(sample)/float(samples);
            if(!storylandSampleModelAnimationLayered(*model,upperAnimation,elapsed,upperAnimation,elapsed,0,
                nullptr,0,0,true,{},points,bones,error,&frames)||frames.size()!=skeleton.size())continue;
            const float extension=(frames[hand][12]-frames[chest][12])*std::cos(forward)+
                (frames[hand][13]-frames[chest][13])*std::sin(forward);
            if(extension>best) {best=extension;time=elapsed;}
        }
        contacts.emplace(key,time);
        return time;
    };
    for(auto& weapon:definitions) {
        if(weapon.fireType!=0)continue;
        for(size_t index=0;index<3;++index) {
            const uint32_t clip=groups.resolve(weapon.nativeId<=0?(groups.vcsLayout?63:7):uint32_t(std::max(0,weapon.animationGroup)),
                weapon.nativeId<=0?moves[index]:storylandRetailMeleeSwingAssociation(
                    groups,weapon.animationGroup,uint32_t(index+1),false),upperAnimation.clips());
            if(clip==UINT32_MAX)continue;
            const float duration=std::max(0.1f,upperAnimation.clips()[clip].duration);
            weapon.meleeDurations[index]=duration;
            const float time=contact(clip,weapon.nativeId<=0&&index!=1?34:24,duration);
            if(time>=0) {weapon.meleeContactTimes[index]=time;weapon.meleeTimingFromMotion=true;}
        }
        const uint32_t running=groups.resolve(weapon.nativeId<=0?(groups.vcsLayout?63:7):uint32_t(std::max(0,weapon.animationGroup)),
            weapon.nativeId<=0?(groups.vcsLayout?319:203):storylandRetailMeleeSwingAssociation(
                groups,weapon.animationGroup,1,true),upperAnimation.clips());
        if(running!=UINT32_MAX) {
            weapon.runningMeleeDuration=std::max(0.1f,upperAnimation.clips()[running].duration);
            const float time=contact(running,24,weapon.runningMeleeDuration);
            if(time>=0) {weapon.runningMeleeContactTime=time;weapon.meleeTimingFromMotion=true;}
        }
    }
}

std::string StorylandPlayTestAnimation::selectedActionBank() const {
    for(const auto& clip:upperAnimation.clips())if(clip.index==upperAction)return clip.bankName;
    return {};
}
std::string StorylandPlayTestAnimation::selectedActionName() const {
    for(const auto& clip:upperAnimation.clips())if(clip.index==upperAction)return clip.name;
    return {};
}

bool StorylandPlayTestAnimation::hasNamedClip(const std::string& name) const {
    const auto requested=clipLower(name);
    for(const auto& clip:animation.clips())
        if(clipMatches(clip.name,requested))return true;
    return false;
}

float StorylandPlayTestAnimation::namedClipDuration(const std::string& name) const {
    const auto requested=clipLower(name);
    for(const auto& clip:animation.clips())
        if(clipMatches(clip.name,requested))return clip.duration;
    return 0;
}

bool StorylandPlayTestAnimation::sampleNamedClip(const StorylandModelFile& model,
    const std::string& name,float seconds,std::vector<StorylandModelPoint>& points,std::string& error) {
    const auto requested=clipLower(name);
    for(const auto& clip:animation.clips()) {
        if(!clipMatches(clip.name,requested))continue;
        if(!animation.setActiveClipIndex(clip.index)||!animation.hasDecodedMotion()) {
            error="Named cutscene animation has no decoded skeleton: "+name;return false;
        }
        const float duration=std::max(0.001f,animation.durationSeconds());
        const float time=std::clamp(seconds,0.0f,duration-0.001f);
        std::vector<StorylandModelPoint> bones;
        return storylandSampleModelAnimation(model,animation,time,true,model.previewPoints(),points,bones,error);
    }
    error="Named cutscene animation was not found: "+name;
    return false;
}

bool StorylandPlayTestAnimation::retailAnimationLoops(int32_t animationId,int32_t group) const {
    if(animationId<0)return false;
    if(group<0) {
        if(groups.vcsLayout)return false;
        group=0;
    }
    const auto* association=groups.find(uint32_t(group),uint32_t(animationId));
    return association&&(association->flags&2)!=0;
}

float StorylandPlayTestAnimation::retailAnimationDuration(int32_t animationId,int32_t group) const {
    if(animationId<0)return 0;
    const uint32_t clipIndex=group>=0?groups.resolve(uint32_t(group),uint32_t(animationId),animation.clips()):
        groups.vcsLayout?uint32_t(animationId):groups.resolve(0,uint32_t(animationId),animation.clips());
    const auto found=std::find_if(animation.clips().begin(),animation.clips().end(),
        [&](const auto& clip){return clip.index==clipIndex;});
    return found==animation.clips().end()?0:std::max(0.0f,found->duration);
}

float StorylandPlayTestAnimation::weaponAnimationDuration(int32_t group,int32_t animationId) const {
    if(group<0||animationId<0)return 0;
    const uint32_t clipIndex=groups.resolve(uint32_t(group),uint32_t(animationId),upperAnimation.clips());
    const auto found=std::find_if(upperAnimation.clips().begin(),upperAnimation.clips().end(),
        [&](const auto& clip){return clip.index==clipIndex;});
    return found==upperAnimation.clips().end()||!std::isfinite(found->duration)?0:std::max(0.0f,found->duration);
}

bool StorylandPlayTestAnimation::sampleRetailAnimationId(const StorylandModelFile& model,
    int32_t animationId,float seconds,std::vector<StorylandModelPoint>& points,std::string& error,int32_t group,
    std::vector<std::array<float,16>>* frames,StorylandPlayTestMotion baseMotion,float basePhase,float associationWeight,
    bool skinGeometry,StorylandAnimationTransition* transition,float blendRate,int32_t associationFlags,
    float transitionSeconds,bool retainAuthoredRoot) {
    const std::vector<StorylandModelPoint> noGeometry;
    const auto& sourcePoints=skinGeometry?model.previewPoints():noGeometry;
    if(frames)frames->clear();
    if(animationId<0||!std::isfinite(seconds)||!std::isfinite(basePhase)) {
        error="Scripted animation ID or sample time is invalid.";return false;
    }
    if(!std::isfinite(associationWeight)||associationWeight<0||associationWeight>1) {
        error="Scripted animation weight must be finite and between zero and one.";return false;
    }
    if(associationFlags < -1 || associationFlags > UINT16_MAX) {
        error="Scripted animation association flags are outside the native16-bit range.";return false;
    }
    if(!std::isfinite(transitionSeconds)||transitionSeconds < -1) {
        error="Animation transition time is invalid.";return false;
    }
    if(!groups.vcsLayout&&group<0)group=0;
    const uint32_t clipIndex=group>=0?groups.resolve(uint32_t(group),uint32_t(animationId),animation.clips()):
        groups.vcsLayout?uint32_t(animationId):groups.resolve(0,uint32_t(animationId),animation.clips());
    if(clipIndex==UINT32_MAX) {
        error="Scripted animation association is unavailable: group "+std::to_string(group)+
            ", animation "+std::to_string(animationId);return false;
    }
    const auto found=std::find_if(animation.clips().begin(),animation.clips().end(),
        [&](const auto& clip){return clip.index==clipIndex;});
    if(found==animation.clips().end()||
        !animation.setActiveClipIndex(found->index)||!animation.hasDecodedMotion()) {
        error="Scripted animation clip has no decoded motion: group "+std::to_string(group)+
            ", animation "+std::to_string(animationId);return false;
    }
    if(transition) {
        transition->key=(uint64_t(2)<<60)|(uint64_t(uint32_t(group))<<28)|uint32_t(animationId);
        transition->seconds=transitionSeconds>=0?transitionSeconds:seconds;
        transition->blendRate=blendRate;
    }
    const float duration=std::max(0.001f,animation.durationSeconds());
    const auto* association=group>=0?groups.find(uint32_t(group),uint32_t(animationId)):nullptr;
    const uint16_t flags=associationFlags>=0?uint16_t(associationFlags):
        association?association->flags:uint16_t(0);
    const float time=flags&2?std::fmod(std::max(0.0f,seconds),duration):
        std::clamp(seconds,0.0f,groups.vcsLayout?duration-0.001f:duration);
    std::vector<StorylandModelPoint> bones;
    const bool partial=(flags&0x10u)!=0;
    const bool mirrored=groups.vcsLayout&&(flags&0x8000u)!=0;
    const auto nativeRootPose=!retainAuthoredRoot?StorylandNativeRootPose::Script:
        groups.vcsLayout?StorylandNativeRootPose::VcsPedestrian:StorylandNativeRootPose::LcsPedestrian;
    if(!groups.vcsLayout||partial||mirrored||associationWeight<1) {
        StorylandPedAimPose aim;aim.transition=transition;
        if(partial||associationWeight<1) {
            const uint32_t baseClip=baseMotion==StorylandPlayTestMotion::Sprint?sprint:
                baseMotion==StorylandPlayTestMotion::Run?run:baseMotion==StorylandPlayTestMotion::Walk?walk:idle;
            if(baseClip==UINT32_MAX||!secondaryAnimation.setActiveClipIndex(baseClip)||!secondaryAnimation.hasDecodedMotion()) {
                error="Partial scripted animation requires the actor's decoded locomotion association.";return false;
            }
            const float baseDuration=std::max(0.001f,secondaryAnimation.durationSeconds());
            const float baseTime=baseMotion!=StorylandPlayTestMotion::Idle&&basePhase>=0?
                std::fmod(basePhase,1.0f)*baseDuration:std::fmod(std::max(0.0f,seconds),baseDuration);
            const std::vector<StorylandAnimationPoseLayer> layers{
                {&animation,time,associationWeight,partial,nullptr,mirrored},
                {&secondaryAnimation,baseTime,partial?1.0f:1.0f-associationWeight,false}};
            if((groups.vcsLayout&&!mirrored)||retainAuthoredRoot) {
                const uint32_t baseId=groups.vcsLayout?
                    (baseMotion==StorylandPlayTestMotion::Sprint?5u:
                        baseMotion==StorylandPlayTestMotion::Run?4u:baseMotion==StorylandPlayTestMotion::Walk?2u:6u):
                    (baseMotion==StorylandPlayTestMotion::Sprint?2u:
                        baseMotion==StorylandPlayTestMotion::Run?1u:baseMotion==StorylandPlayTestMotion::Walk?0u:3u);
                const auto* baseAssociation=groups.find(uint32_t(std::max(0,locomotionGroup)),baseId);
                return storylandSampleNativeRootAssociations(model,layers,
                    {flags,baseAssociation?baseAssociation->flags:uint16_t(0)},
                    sourcePoints,points,bones,error,frames,transition,nativeRootPose);
            }
            return storylandSampleNativeWeaponAssociations(model,layers,aim,
                sourcePoints,points,bones,error,frames);
        }
        if(retainAuthoredRoot)
            return storylandSampleNativeRootAssociations(model,{{&animation,time,1,false,nullptr,mirrored}},
                {flags},sourcePoints,points,bones,error,frames,transition,nativeRootPose);
        return storylandSampleNativeWeaponAssociations(model,{{&animation,time,1,false,nullptr,mirrored}},aim,
            sourcePoints,points,bones,error,frames);
    }
    return storylandSampleNativeRootAssociations(model,{{&animation,time,1,false}},
        {flags},sourcePoints,points,bones,error,frames,transition,nativeRootPose);
}

bool StorylandPlayTestAnimation::sampleVcsBoatDriverPose(const StorylandModelFile& model,
    const StorylandVehicleDefinition& definition,std::vector<StorylandNativePedBoneFrame>& frames,
    std::string& error) {
    frames.clear();
    if(!groups.vcsLayout||!definition.vcsModelLayout||definition.nativeType!=1||
        !definition.hasNativeHandling||(definition.modelFlags&0x400u)==0) {
        error="Native boat-driver pose requires the original CAR_sit selector.";return false;
    }
    const auto* association=groups.find(0,97);
    const uint32_t clip=groups.resolve(0,97,animation.clips());
    if(!association||(association->flags&0x80D0u)!=0||clip==UINT32_MAX||
        !animation.setActiveClipIndex(clip)||!animation.hasDecodedMotion()) {
        error="Native boat-driver CAR_sit association is unavailable.";return false;
    }
    for(const auto& track:animation.tracks()) {
        if(track.keys.empty())continue;
        const auto& first=track.keys.front();
        for(const auto& key:track.keys) {
            if(key.qx!=first.qx||key.qy!=first.qy||key.qz!=first.qz||key.qw!=first.qw||
                ((track.channelFlags&2u)&&(key.tx!=first.tx||key.ty!=first.ty||key.tz!=first.tz))) {
                error="Native boat-driver CAR_sit requires its original constant pose.";return false;
            }
        }
    }
    return storylandSampleVcsPedEntityPose(model,animation,0,frames,error);
}

bool StorylandPlayTestAnimation::sampleDeathAnimation(const StorylandModelFile& model,
    int32_t animationId,float seconds,std::vector<StorylandModelPoint>& points,std::string& error,
    std::vector<std::array<float,16>>* frames,bool skinGeometry,
    StorylandAnimationTransition* transition,float blendRate,float speed) {
    lastDeathClipSampled=false;
    if(!std::isfinite(seconds)||!std::isfinite(blendRate)||!std::isfinite(speed)||
        blendRate<0||speed<=0) {error="Invalid native death association.";return false;}
    if(animationId<0)animationId=groups.vcsLayout?16:13;
    const float duration=retailAnimationDuration(animationId,0);
    if(duration<=0) {error="Native death association is unavailable.";return false;}
    const float elapsed=std::clamp(std::max(0.0f,seconds)*speed,0.0f,
        groups.vcsLayout?std::max(0.0f,duration-.001f):duration);
    // UpdateBlend keeps advancing after UpdateTime stops at the last key.
    if(transition&&transition->initialized) {
        const uint64_t key=(uint64_t(2)<<60)|uint32_t(animationId);
        if(transition->appliedKey!=key||seconds+1e-5f<transition->lastSeconds)
            transition->sourceLocal=transition->previousLocal;
        transition->weight=std::clamp(std::max(0.0f,seconds)*blendRate,0.0f,1.0f);
        transition->appliedKey=key;
        transition->lastSeconds=std::max(0.0f,seconds);
    }
    lastDeathClipSampled=sampleRetailAnimationId(model,animationId,elapsed,points,error,0,frames,
        StorylandPlayTestMotion::Idle,-1,1,skinGeometry,transition,blendRate,-1,std::max(0.0f,seconds),
        !model.isMobileLcsDff()&&!model.isManhuntModel());
    return lastDeathClipSampled;
}

bool StorylandPlayTestAnimation::sampleScriptAnimation(const StorylandModelFile& model,
    int32_t animationId,int32_t group,float seconds,float weight,StorylandAnimationBlendState& blend,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry) {
    return sampleScriptAnimation(model,animationId,group,seconds,weight,blend,points,error,
        skinGeometry,StorylandPlayTestMotion::Idle,-1);
}

bool StorylandPlayTestAnimation::sampleScriptAnimation(const StorylandModelFile& model,
    int32_t animationId,int32_t group,float seconds,float weight,StorylandAnimationBlendState& blend,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry,
    StorylandPlayTestMotion baseMotion,float basePhase) {
    blend.boneFrames.clear();
    blend.poseTransition.key=(uint64_t(2)<<60)|(uint64_t(uint32_t(group))<<28)|uint32_t(animationId);
    blend.poseTransition.seconds=seconds;
    blend.poseTransition.blendRate=8;
    if(animationId<0||!std::isfinite(seconds)||!std::isfinite(weight)||weight<0) {
        error="Scripted animation ID, sample time or blend weight is invalid.";return false;
    }
    return sampleRetailAnimationId(model,animationId,seconds,points,error,group,&blend.boneFrames,
        baseMotion,basePhase,std::min(weight,1.0f),skinGeometry,&blend.poseTransition);
}

bool StorylandPlayTestAnimation::sampleCrouchPose(const StorylandModelFile& model,
    int32_t animationId,int32_t group,float seconds,float weight,StorylandAnimationBlendState& blend,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry) {
    const std::vector<StorylandModelPoint> noGeometry;
    const auto& sourcePoints=skinGeometry?model.previewPoints():noGeometry;
    const uint32_t pose=groups.resolve(uint32_t(group),uint32_t(animationId),animation.clips());
    const bool stance=animationId==153||animationId==154||animationId==155;
    const uint32_t base=stance?idle:groups.resolve(0,155,secondaryAnimation.clips());
    if(pose==UINT32_MAX||base==UINT32_MAX||!animation.setActiveClipIndex(pose)||
        !secondaryAnimation.setActiveClipIndex(base)||!animation.hasDecodedMotion()||!secondaryAnimation.hasDecodedMotion()) {
        error="Native crouch association has not been decoded.";return false;
    }
    const float end=std::max(0.0f,animation.durationSeconds()-.001f);
    const float baseTime=stance?0:std::max(0.0f,secondaryAnimation.durationSeconds()-.001f);
    std::vector<StorylandModelPoint> bones;
    return storylandSampleModelAnimationRelativeRoot(model,secondaryAnimation,baseTime,
        animation,std::clamp(seconds,0.0f,end),std::clamp(weight,0.0f,1.0f),sourcePoints,
        points,bones,blend.boneFrames,error,!stance);
}

bool StorylandPlayTestAnimation::sampleExternalNamedClip(const StorylandModelFile& model,
    StorylandAnimFile& external,const std::string& clipName,float seconds,bool loop,
    std::vector<StorylandModelPoint>& points,std::string& error,
    std::vector<std::array<float,16>>* frames,bool preserveRootVertical) {
    for(const auto& clip:external.clips()) {
        if(clipKey(clip.name)!=clipKey(clipName))continue;
        if(!external.setActiveClipIndex(clip.index)||!external.hasDecodedMotion())break;
        const float duration=std::max(0.001f,external.durationSeconds());
        const float elapsed=loop?std::fmod(std::max(0.0f,seconds),duration):
            std::clamp(seconds,0.0f,duration-0.0001f);
        std::vector<StorylandModelPoint> bones;
        return storylandSampleModelAnimationLayered(model,external,elapsed,external,elapsed,
            0.0f,nullptr,0.0f,0.0f,true,model.previewPoints(),points,bones,error,frames,
            nullptr,preserveRootVertical);
    }
    error="The requested external animation is not decoded: "+clipName;
    return false;
}

bool StorylandPlayTestAnimation::sampleExternalClimbPose(const StorylandModelFile& model,
    StorylandAnimFile& external,const std::string& clipName,float seconds,bool loop,float sampleTime,
    StorylandAnimationBlendState& blend,std::vector<StorylandModelPoint>& points,std::string& error) {
    if(!std::isfinite(seconds)||!std::isfinite(sampleTime)) {
        error="Climb animation time must be finite.";return false;
    }
    for(const auto& clip:external.clips()) {
        if(clipKey(clip.name)!=clipKey(clipName))continue;
        if(!external.setActiveClipIndex(clip.index)||!external.hasDecodedMotion())break;
        const float duration=std::max(.001f,external.durationSeconds());
        const float time=loop?std::fmod(std::max(0.0f,seconds),duration):
            std::clamp(seconds,0.0f,duration-.0001f);
        blend.poseTransition.key=(uint64_t(10)<<60)|clip.index;
        blend.poseTransition.seconds=sampleTime;
        blend.poseTransition.blendRate=8.0f;
        std::vector<StorylandModelPoint> bones;
        return storylandSampleModelAnimationLayered(model,external,time,external,time,0,
            nullptr,0,0,true,model.previewPoints(),points,bones,error,&blend.boneFrames,
            &blend.poseTransition,false);
    }
    error="The requested climb animation is not decoded: "+clipName;return false;
}

bool StorylandPlayTestAnimation::sampleExternalCrouchPose(const StorylandModelFile& model,
    StorylandAnimFile& external,const std::string& clipName,float seconds,float stanceWeight,
    float moveWeight,std::vector<StorylandModelPoint>& points,std::string& error,
    std::vector<std::array<float,16>>* frames) {
    return sampleExternalCrouchPose(model,external,clipName,seconds,stanceWeight,moveWeight,
        points,error,frames,nullptr,0.0f);
}

bool StorylandPlayTestAnimation::sampleExternalCrouchPose(const StorylandModelFile& model,
    StorylandAnimFile& external,const std::string& clipName,float seconds,float stanceWeight,
    float moveWeight,std::vector<StorylandModelPoint>& points,std::string& error,
    std::vector<std::array<float,16>>* frames,StorylandAnimationBlendState* weaponState,float sampleTime) {
    if(!std::isfinite(seconds)||!std::isfinite(stanceWeight)||!std::isfinite(moveWeight)) {
        error="External crouch sample has a non-finite time or blend weight.";return false;
    }
    stanceWeight=std::clamp(stanceWeight,0.0f,1.0f);
    moveWeight=std::clamp(moveWeight,0.0f,1.0f);
    const StorylandAnimFile* upper=nullptr;
    float upperTime=0,upperWeight=0;
    StorylandPedAimPose aim;
    if(weaponState && std::isfinite(sampleTime)) {
        auto& state=*weaponState;
        const float delta=std::clamp(sampleTime-state.lastSampleTime,0.0f,0.1f);
        state.lastSampleTime=sampleTime;
        bool action=state.upperActive&&upperAction!=UINT32_MAX;
        if(action) {
            upperTime=std::max(0.0f,sampleTime-state.upperStartedAt);
            if(!state.weaponReloading)upperTime+=firePoseStart;
            const float end=state.weaponReloading?reloadDuration:firePoseEnd;
            if(end<=0||upperTime>=end) {state.upperActive=false;action=false;}
        }
        const bool aiming=state.weaponAiming&&aimAction!=UINT32_MAX;
        const bool requested=aiming||action;
        if(requested&&!state.weaponAimWasActive&&state.weaponAimBlend==0)state.weaponAimTime=0;
        state.weaponAimWasActive=requested;
        state.weaponAimBlend=std::clamp(state.weaponAimBlend+(requested?8.0f:-8.0f)*delta,0.0f,1.0f);
        if(aiming)state.weaponAimTime=std::min(aimPoseTime,state.weaponAimTime+delta);
        const uint32_t selected=action?upperAction:aimAction;
        if(state.weaponAimBlend>0&&selected!=UINT32_MAX&&upperAnimation.setActiveClipIndex(selected)&&upperAnimation.hasDecodedMotion()) {
            upper=&upperAnimation;
            if(!action)upperTime=std::min(state.weaponAimTime,aimPoseTime);
            upperWeight=state.weaponAimBlend;
        }
        aim.state=&state.weaponAimIk;
        aim.yaw=state.weaponAimYaw;aim.pitch=state.weaponAimPitch;aim.seconds=delta;
        aim.arm=weaponUsesArmAim;aim.modelForwardAngle=storylandPedModelForwardAngle(model);
        aim.aiming=state.weaponAiming&&state.weaponAimDirectionValid&&!state.weaponReloading;
        aim.weight=!state.weaponReloading?state.weaponAimBlend:0;
    }
    const auto grounded=[&](bool sampled) {
        return sampled&&storylandPlantRetargetedCrouchFeet(model,points,frames,error);
    };
    const auto stance=std::find_if(external.clips().begin(),external.clips().end(),
        [](const auto& clip){return clipKey(clip.name)==clipKey("WEAPON_crouch");});
    if(stance==external.clips().end()||!external.setActiveClipIndex(stance->index)||
        !external.hasDecodedMotion()) {
        error="External crouch stance clip is unavailable.";return false;
    }
    if(stanceWeight<1.0f) {
        if(!animation.setActiveClipIndex(idle)||!animation.hasDecodedMotion()) {
            error="Native standing association is unavailable for crouch transition.";return false;
        }
        return grounded(storylandSampleRetargetedCrouchTransitionAim(model,animation,external,0,
            stanceWeight,model.previewPoints(),points,error,frames,upper,upperTime,upperWeight,
            weaponState?&aim:nullptr));
    }
    StorylandAnimFile stanceAnimation=external;
    if(moveWeight<=0||clipKey(clipName)==clipKey("WEAPON_crouch")) {
        std::vector<StorylandModelPoint> bones;
        return grounded(storylandSampleModelAnimationLayeredAim(model,stanceAnimation,0,stanceAnimation,0,0,
            upper,upperTime,upperWeight,true,model.previewPoints(),points,bones,error,frames,nullptr,true,
            weaponState?&aim:nullptr));
    }
    const auto movement=std::find_if(external.clips().begin(),external.clips().end(),
        [&](const auto& clip){return clipKey(clip.name)==clipKey(clipName);});
    if(movement==external.clips().end()||!external.setActiveClipIndex(movement->index)||
        !external.hasDecodedMotion()) {
        error="External crouch movement clip is unavailable: "+clipName;return false;
    }
    const float duration=std::max(.001f,external.durationSeconds());
    const float elapsed=std::fmod(std::max(0.0f,seconds),duration);
    std::vector<StorylandModelPoint> bones;
    return grounded(storylandSampleModelAnimationLayeredAim(model,stanceAnimation,0,external,elapsed,
        moveWeight,upper,upperTime,upperWeight,true,model.previewPoints(),points,bones,error,frames,nullptr,true,
        weaponState?&aim:nullptr));
}

float StorylandPlayTestAnimation::selectedWeaponClipDuration() const {
    return upperAnimation.hasDecodedMotion()?upperAnimation.durationSeconds():0.0f;
}

bool StorylandPlayTestAnimation::sampleExternalClip(const StorylandModelFile& model,
    StorylandAnimFile& external,float seconds,std::vector<StorylandModelPoint>& points,std::string& error) {
    if(external.clips().empty()) {error="The external mocap bank has no animation clips.";return false;}
    bool selected=false;
    for(const auto& clip:external.clips()) {
        const std::string key=clipKey(clip.name);
        if(key!="csplr"&&key!="player"&&key!="csplayer"&&key!="plr")continue;
        if(external.setActiveClipIndex(clip.index)&&external.hasDecodedMotion()) {
            selected=true;break;
        }
    }
    if(!selected) {
        for(const auto& clip:external.clips())
            if(external.setActiveClipIndex(clip.index)&&external.hasDecodedMotion()) {selected=true;break;}
        if(!selected) {error="The external mocap has no decoded motion tracks.";return false;}
    }
    const float duration=std::max(0.001f,external.durationSeconds());
    const float time=std::clamp(seconds,0.0f,duration-0.001f);
    std::vector<StorylandModelPoint> bones;
    return storylandSampleModelAnimation(model,external,time,true,model.previewPoints(),points,bones,error);
}

float StorylandPlayTestAnimation::vehicleActionDuration(const std::string& action,bool low,bool rightHandSide,bool requireNative) const {
    StorylandVehicleAnimationOptions options;options.low=low;options.rightHandSide=rightHandSide;
    return vehicleActionDuration(action,options,requireNative);
}
float StorylandPlayTestAnimation::vehicleActionDuration(const std::string& action,
    const StorylandVehicleAnimationOptions& options,bool requireNative) const {
    if(action=="close_inside"&&options.variant==StorylandVehicleEntryVariant::PassengerSideDriver) {
        auto ordinary=options;ordinary.variant=StorylandVehicleEntryVariant::Normal;
        return (options.doorMissing?0:vehicleActionDuration(action,ordinary,requireNative))+
            vehicleActionDuration("shuffle",ordinary,requireNative);
    }
    const uint32_t id=vehicleAssociation(action,options,groups);
    const auto index=groups.resolve(vehicleAssociationGroup(action,options),id,animation.clips());
    for(const auto& clip:animation.clips())if(clip.index==index&&std::isfinite(clip.duration)&&
        clip.duration>(action=="sit"?.001f:.05f)&&clip.duration<10)return clip.duration;
    // No invented animation length: an absent retail association is unavailable.
    return 0.0f;
}
StorylandVehicleAnimationOptions StorylandPlayTestAnimation::vehicleAnimationOptions(
    const StorylandVehicleDefinition& definition,const storyland_vcs_jetski_pose::Sample* jetSkiPose,
    std::string* liveVehicleError) const {
    StorylandVehicleAnimationOptions options;
    options.low=definition.lowVehicle();options.boat=definition.domain==StorylandVehicleDomain::Water;
    options.twoWheels=definition.twoWheels();
    options.vcsHelicopter=storylandVcsHelicopter(definition);
    options.vcsHovercraft=definition.vcsHovercraft();
    options.vcsJetSki=definition.vcsJetSki();
    options.vcsOrdinaryBoat=storylandVcsOrdinaryBoat(definition);
    if(options.vcsOrdinaryBoat)options.boat=(definition.modelFlags&0x400u)==0;
    options.jetSkiPose=jetSkiPose;options.liveVehicleError=liveVehicleError;
    if(options.vcsJetSki)options.animationGroup=int32_t(StorylandVcsJetSkiAnimationGroup);
    options.jumpIn=options.vcsHovercraft&&(definition.modelFlags&0x4000u)!=0&&
        (definition.modelFlags&0x200u)==0;
    options.quad=definition.vcsModelLayout&&definition.nativeType==9;
    options.van=definition.vanVehicle();
    options.bus=definition.busVehicle();
    if(options.twoWheels||options.quad) {
        options.animationGroup=-1;
        const auto found=vehicleAnimationGroups.find(definition.modelId);
        if(found!=vehicleAnimationGroups.end())options.animationGroup=int32_t(found->second);
        else {
            const std::string bank=options.quad?"quad":definition.bicycle()?"bmx":"bikes";
            for(const auto& entry:groups.groups)if(entry.second.bank==bank) {
                options.animationGroup=int32_t(entry.first);break;
            }
        }
    }
    return options;
}
void StorylandPlayTestAnimation::configureVehicleAnimations(StorylandVehicleDefinition& definition) {
    using Variant=StorylandVehicleEntryVariant;
    const auto options=vehicleAnimationOptions(definition);
    const bool rider=options.twoWheels||options.quad;
    if(!definition.vcsModelLayout&&definition.motorcycle()) {
        const auto previousClip=animation.activeClipIndex();
        auto selectedOptions=options;
        selectedOptions.variant=Variant::Normal;
        selectedOptions.rightHandSide=false;
        const auto association=vehicleAssociation("enter",selectedOptions,groups);
        const auto group=vehicleAssociationGroup("enter",selectedOptions);
        const auto clip=groups.resolve(group,association,animation.clips());
        std::array<float,3> root{};
        bool available=clip!=UINT32_MAX&&animation.setActiveClipIndex(clip)&&!animation.tracks().empty();
        if(available) {
            const auto& sequence=animation.tracks().front();
            available=!sequence.keys.empty();
            if(available&&(sequence.channelFlags&2u)) {
                const auto& last=sequence.keys.back();
                root={last.tx,last.ty,last.tz};
                available=std::all_of(root.begin(),root.end(),[](float value){return std::isfinite(value);});
            }
        }
        animation.setActiveClipIndex(previousClip);
        definition.hasNativeMotorcycleEntryRoot=available;
        definition.nativeMotorcycleEntryRoot=available?root:decltype(root){};
    }
    if(!definition.vcsModelLayout&&definition.nativeType==0) {
        const auto previousClip=animation.activeClipIndex();
        bool available=true;
        std::array<std::array<float,3>,2> roots{};
        for(unsigned rear=0;rear<2;++rear) {
            auto selectedOptions=options;
            selectedOptions.rightHandSide=false;
            selectedOptions.rearDoor=rear!=0;
            selectedOptions.bus=false;
            selectedOptions.jumpIn=false;
            const auto association=vehicleAssociation("enter",selectedOptions,groups);
            const auto group=vehicleAssociationGroup("enter",selectedOptions);
            const auto clip=groups.resolve(group,association,animation.clips());
            if(clip==UINT32_MAX||!animation.setActiveClipIndex(clip)||animation.tracks().empty()) {
                available=false;break;
            }
            const auto& sequence=animation.tracks().front();
            if(sequence.keys.empty()) {available=false;break;}
            if(sequence.channelFlags&2u) {
                const auto& last=sequence.keys.back();
                roots[rear]={last.tx,last.ty,last.tz};
                if(!std::all_of(roots[rear].begin(),roots[rear].end(),
                    [](float value){return std::isfinite(value);})) {available=false;break;}
            }
        }
        animation.setActiveClipIndex(previousClip);
        definition.hasNativeDoorRoots=available;
        definition.nativeDoorRoots=available?roots:decltype(roots){};
    }
    if(options.vcsOrdinaryBoat) {
        // 301C7C/302430 finish type1 transitions directly; 30AAC8 selects the real seated pose.
        definition.entrySeconds=definition.rightEntrySeconds=0;
        definition.exitSeconds=definition.rightExitSeconds=definition.rearExitSeconds=0;
        definition.jackSeconds=definition.frontEntrySeconds=definition.shuffleSeconds=0;
        definition.pickupSeconds={};definition.pullupSeconds={};definition.crawlExitSeconds={};
        definition.movingExitSeconds={};definition.movingExitAssociations={-1,-1};
        definition.movingExitGroups={-1,-1};
        definition.transitionTiming.seconds.fill(0);
        for(auto& timing:definition.sideTransitionTiming)timing.seconds.fill(0);
        definition.retailTransitions=storylandVcsBoatEntryAvailable(definition)&&
            vehicleActionDuration("sit",options,true)>0&&retailAnimationDuration(6,0)>0;
        return;
    }
    definition.entrySeconds=vehicleActionDuration("enter",options,true);
    definition.exitSeconds=vehicleActionDuration("exit",options,true);
    definition.jackSeconds=vehicleActionDuration("hijack",options,true);
    auto right=options;right.rightHandSide=true;
    definition.rightEntrySeconds=vehicleActionDuration("enter",right,true);
    definition.rightExitSeconds=vehicleActionDuration("exit",right,true);
    auto rear=options;rear.rearExit=true;
    definition.rearExitSeconds=rider?vehicleActionDuration("exit",rear,true):0;
    auto special=options;special.variant=Variant::BikeFront;
    definition.frontEntrySeconds=options.twoWheels?vehicleActionDuration("enter",special,true):0;
    definition.shuffleSeconds=rider||options.vcsJetSki?0:vehicleActionDuration("shuffle",options,true);
    definition.vcsJetSkiHighAlignSeconds={};definition.vcsJetSkiWetExitSeconds={};
    for(unsigned side=0;side<2;++side) {
        special=options;special.rightHandSide=side!=0;
        special.variant=Variant::BikePickup;
        definition.pickupSeconds[side]=options.twoWheels?vehicleActionDuration("open",special,true):0;
        special.variant=Variant::BikePullup;
        definition.pullupSeconds[side]=options.twoWheels?vehicleActionDuration("open",special,true):0;
        special.variant=Variant::CrawlOut;
        definition.crawlExitSeconds[side]=options.twoWheels?0:vehicleActionDuration("exit",special,true);
        special.variant=Variant::MovingExit;
        definition.movingExitSeconds[side]=vehicleActionDuration("exit",special,true);
        definition.movingExitAssociations[side]=int32_t(vehicleAssociation("exit",special,groups));
        definition.movingExitGroups[side]=int32_t(vehicleAssociationGroup("exit",special));
    }
    definition.transitionTiming={};
    definition.sideTransitionTiming={};
    for(unsigned side=0;side<2;++side) {
        auto sided=options;sided.rightHandSide=side!=0;
        definition.sideTransitionTiming[side].seconds[unsigned(StorylandVehiclePhase::PullOut)]=
            vehicleActionDuration("hijack",sided,true);
    }
    const bool vcsHelicopter=definition.vcsModelLayout&&
        definition.domain==StorylandVehicleDomain::Air&&definition.nativeType==4;
    const bool roadVehicle=definition.domain==StorylandVehicleDomain::Road&&
        (definition.nativeType==0||rider);
    definition.retailTransitions=(roadVehicle||vcsHelicopter||options.vcsHovercraft||options.vcsJetSki)&&
        definition.entrySeconds>0&&definition.exitSeconds>0&&
        vehicleActionDuration("sit",options,true)>0;
    if(definition.retailTransitions&&!rider) {
        for(unsigned side=0;side<2;++side) {
            auto sided=options;sided.rightHandSide=side!=0;
            for(const auto phase:{StorylandVehiclePhase::Align,StorylandVehiclePhase::Open,
                StorylandVehiclePhase::CloseInside,StorylandVehiclePhase::CloseOutside}) {
                if((vcsHelicopter||options.vcsHovercraft)&&phase==StorylandVehiclePhase::Align)continue;
                if(options.vcsJetSki&&phase!=StorylandVehiclePhase::Align)continue;
                if(options.bus&&(phase==StorylandVehiclePhase::CloseInside||phase==StorylandVehiclePhase::CloseOutside))continue;
                definition.sideTransitionTiming[side].seconds[unsigned(phase)]=
                    vehicleActionDuration(storylandVehiclePhaseAction(phase),sided,true);
            }
        }
        definition.transitionTiming=definition.sideTransitionTiming[0];
    }
    definition.vcsJetSkiAnimations={};
    if(options.vcsJetSki) {
        auto& metadata=definition.vcsJetSkiAnimations;
        metadata.available=true;
        for(size_t item=0;item<storyland_vcs_jetski_pose::Ids.size();++item) {
            const int32_t id=storyland_vcs_jetski_pose::Ids[item];
            uint16_t flags=0;
            const float duration=retailAnimationDuration(id,StorylandVcsJetSkiAnimationGroup);
            if(!retailAnimationAssociationFlags(id,StorylandVcsJetSkiAnimationGroup,flags)||
                !std::isfinite(duration)||duration<=0||duration>600)metadata.available=false;
            metadata.clips[item]={duration,flags};
        }
        definition.retailTransitions=definition.retailTransitions&&metadata.available&&definition.hasNativeDummies&&
            definition.hasVcsJetSkiEntryRoots&&definition.collision&&
            definition.rightEntrySeconds>0&&definition.rightExitSeconds>0;
        for(unsigned side=0;side<2;++side) {
            auto selected=options;selected.rightHandSide=side!=0;selected.highAlign=true;
            definition.vcsJetSkiHighAlignSeconds[side]=vehicleActionDuration("align",selected,true);
            selected.highAlign=false;selected.strongWaterContact=true;
            definition.vcsJetSkiWetExitSeconds[side]=vehicleActionDuration("exit",selected,true);
            definition.retailTransitions=definition.retailTransitions&&
                definition.sideTransitionTiming[side].seconds[unsigned(StorylandVehiclePhase::Align)]>0&&
                definition.vcsJetSkiHighAlignSeconds[side]>0&&definition.vcsJetSkiWetExitSeconds[side]>0;
        }
    }
}
bool StorylandPlayTestAnimation::sampleVehicleAction(const StorylandModelFile& model,
    const std::string& action,float fraction,std::vector<StorylandModelPoint>& points,std::string& error) {
    return sampleVehicleAction(model,action,fraction,{},points,error);
}
bool StorylandPlayTestAnimation::sampleVehicleAction(const StorylandModelFile& model,
    const std::string& action,float fraction,const StorylandVehicleAnimationOptions& options,
    std::vector<StorylandModelPoint>& points,std::string& error,StorylandVehicleRootMotion* root,
    std::vector<std::array<float,16>>* frames) {
    if(frames)frames->clear();
    if(action=="close_inside"&&options.variant==StorylandVehicleEntryVariant::PassengerSideDriver) {
        auto ordinary=options;ordinary.variant=StorylandVehicleEntryVariant::Normal;
        const float shuffle=vehicleActionDuration("shuffle",ordinary,true);
        const float close=options.doorMissing?0:vehicleActionDuration("close_inside",ordinary,true);
        const float elapsed=std::clamp(fraction,0.0f,1.0f)*(shuffle+close);
        if(close>0&&elapsed<close)
            return sampleVehicleAction(model,"close_inside",elapsed/close,ordinary,points,error,root,frames);
        return sampleVehicleAction(model,"shuffle",shuffle>0?(elapsed-close)/shuffle:1,
            ordinary,points,error,root,frames);
    }
    const bool seated=action=="sit";
    const bool helicopterBailout=options.vcsHelicopter&&action=="exit"&&
        options.variant==StorylandVehicleEntryVariant::MovingExit;
    const bool rootMotion=options.rootMotion||helicopterBailout;
    auto* transition=helicopterBailout?nullptr:options.transition;
    const uint32_t id=vehicleAssociation(action,options,groups);
    const uint32_t group=vehicleAssociationGroup(action,options);
    const auto* association=groups.find(group,id);
    uint32_t selected=groups.resolve(group,id,animation.clips());
    if(groups.groups.empty()&&!options.twoWheels&&!options.quad&&!options.vcsJetSki&&!helicopterBailout) {
        const std::string name=seated?(options.boat?"drive_boat":options.low?"car_lsit":"car_sit"):
            action=="enter"?(options.low?"car_getinl_rhs":"car_getin_rhs"):
            action=="exit"?(options.low?"car_getoutl_rhs":"car_getout_rhs"):
            action=="align"?"car_align_rhs":action=="open"?"car_open_rhs":
            action=="close_inside"?(options.low?"car_closedoorl_rhs":"car_closedoor_rhs"):
            action=="close_outside"?"car_close_rhs":action=="hijack"?(options.low?"car_pulloutl_rhs":"car_pullout_rhs"):
            action=="jacked"?(options.low?"car_ljackedrhs":"car_jackedrhs"):"";
        for(const auto& clip:animation.clips())if(clipMatches(clip.name,name)) {selected=clip.index;break;}
    }
    if(selected==UINT32_MAX||!animation.setActiveClipIndex(selected)||!animation.hasDecodedMotion()) {
        error="Retail vehicle association is missing: "+action;return false;
    }
    if(transition) {
        transition->key=(uint64_t(3)<<60)|(uint64_t(group)<<28)|selected;
        transition->seconds=options.elapsedSeconds;
        transition->blendRate=8;
    }
    const float endTime=helicopterBailout?animation.durationSeconds():
        std::max(0.0f,animation.durationSeconds()-.001f);
    const float time=seated?0:std::clamp(fraction,0.0f,1.0f)*endTime;
    const std::vector<StorylandModelPoint> noGeometry;
    const auto& sourcePoints=options.skinGeometry?model.previewPoints():noGeometry;
    std::vector<StorylandModelPoint> bones;
    bool sampled=false;
    if(seated&&options.vcsJetSki&&!options.passenger) {
        const auto fail=[&](const std::string& reason) {
            error="Native VCS JetSki driving pose: "+reason;
            if(options.liveVehicleError)*options.liveVehicleError=error;
            return false;
        };
        const auto* pose=options.jetSkiPose;
        if(!pose||!pose->available||pose->count==0||pose->count>pose->layers.size())
            return fail("the current physical association sample is unavailable.");
        std::vector<StorylandAnimationPoseLayer> layers;
        layers.reserve(pose->count);
        for(size_t item=0;item<pose->count;++item) {
            const auto& layer=pose->layers[item];
            const uint32_t clip=groups.resolve(StorylandVcsJetSkiAnimationGroup,
                uint32_t(layer.animation),animation.clips());
            const auto* native=groups.find(StorylandVcsJetSkiAnimationGroup,uint32_t(layer.animation));
            if(!native||clip>=animation.clips().size()||!std::isfinite(layer.time)||
                !std::isfinite(layer.weight)||layer.time<0||layer.weight<0||layer.weight>1)
                return fail("an original association or its finite sample is unavailable.");
            auto tracks=locomotionTracks.find(clip);
            if(tracks==locomotionTracks.end()) {
                if(!secondaryAnimation.setActiveClipIndex(clip)||!secondaryAnimation.hasDecodedMotion())
                    return fail("an original driving clip has no decoded tracks.");
                tracks=locomotionTracks.emplace(clip,secondaryAnimation.tracks()).first;
            }
            layers.push_back({&animation,layer.time,layer.weight,(native->flags&0x10u)!=0,
                &tracks->second,(native->flags&0x8000u)!=0});
        }
        sampled=storylandSampleLcsAnimationAssociations(model,layers,!rootMotion,
            sourcePoints,points,bones,error,frames);
        if(!sampled)return fail(error);
    }
    if(seated&&!options.passenger&&!options.vcsJetSki&&
        (std::fabs(options.steering)>.01f||options.reversing)) {
        const uint32_t drive=options.twoWheels||options.quad?id+(options.reversing?6u:options.steering>0?2u:3u):
            options.reversing?(options.boat?113:109):
            options.boat?(options.steering>0?111:112):options.low?(options.steering>0?103:104):(options.steering>0?101:102);
        const uint32_t second=groups.resolve(group,drive,secondaryAnimation.clips());
        if(second!=UINT32_MAX&&secondaryAnimation.setActiveClipIndex(second)&&secondaryAnimation.hasDecodedMotion()) {
            const float duration=std::max(.001f,secondaryAnimation.durationSeconds());
            const float phase=(options.twoWheels||options.quad)&&options.reversing?
                std::fmod(std::max(0.0f,options.elapsedSeconds),duration):0;
            if(transition)transition->key^=uint64_t(second)<<12;
            sampled=storylandSampleModelAnimationLayered(model,animation,0,secondaryAnimation,phase,
                options.reversing?1.0f:std::clamp(std::fabs(options.steering),0.0f,1.0f),nullptr,0,0,true,
                sourcePoints,points,bones,error,frames,transition);
        }
    }
    if(!sampled)sampled=association&&(association->flags&0x8000u)?
        storylandSampleMirroredModelAnimation(model,animation,time,!rootMotion,sourcePoints,points,bones,error,frames,transition):
        storylandSampleModelAnimationLayered(model,animation,time,animation,time,0,nullptr,0,0,
            !rootMotion,sourcePoints,points,bones,error,frames,transition);
    if(sampled&&!bones.empty()) {
        size_t pelvis=0;
        for(size_t i=0;i<model.armatureBones().size();++i)if(model.armatureBones()[i].boneId==1) {pelvis=i;break;}
        if(pelvis<bones.size()) {
            if(root) {
                root->current=bones[pelvis];
                std::vector<StorylandModelPoint> ignored,endpoint;
                const auto endpointPose=[&](float at,StorylandModelPoint& value) {
                    const bool ok=association&&(association->flags&0x8000u)?
                        storylandSampleMirroredModelAnimation(model,animation,at,!rootMotion,{},ignored,endpoint,error):
                        storylandSampleModelAnimation(model,animation,at,!rootMotion,{},ignored,endpoint,error);
                    if(ok&&pelvis<endpoint.size())value=endpoint[pelvis];
                    return ok;
                };
                // The mesh may blend from the previous association, but the
                // door/seat warp must compare three samples of this clip.
                // Mixing a blended current root with unblended endpoints
                // introduces a sideways jump each time the clip changes.
                if(rootMotion&&!seated&&transition&&!endpointPose(time,root->current))return false;
                if(!endpointPose(0,root->start)||!endpointPose(endTime,root->end))return false;
            }
            if(options.seatRelative&&!helicopterBailout) {
                for(auto& point:points) {point.x-=bones[pelvis].x;point.y-=bones[pelvis].y;point.z-=bones[pelvis].z;}
                if(frames)for(auto& frame:*frames) {
                    frame[12]-=bones[pelvis].x;frame[13]-=bones[pelvis].y;frame[14]-=bones[pelvis].z;
                }
            }
        }
    }
    return sampled;
}

bool StorylandPlayTestAnimation::sampleSeatedDriveBy(const StorylandModelFile& model,
    int32_t vehicle,int32_t seat,const StorylandNativePoint& localTarget,float seconds,
    StorylandAnimationBlendState& blend,std::vector<StorylandModelPoint>& points,std::string& error,
    bool skinGeometry) {
    auto& state=blend.seatedDriveBy;
    if(!groups.vcsLayout||vehicle<0||seat<0||!std::isfinite(seconds)||
        !std::isfinite(localTarget.x)||!std::isfinite(localTarget.y)||!std::isfinite(localTarget.z)) {
        state.ready=false;error="Native passenger drive-by pose has invalid inputs.";return false;
    }
    const bool left=seat==1||seat==3;
    if(state.vehicle!=vehicle||state.seat!=seat||state.association<0||seconds<state.startedAt) {
        state={};state.vehicle=vehicle;state.seat=seat;state.association=left?242:235;
        state.startedAt=seconds;state.targetLeft=left;
        blend.poseTransition={};
    }
    const auto moving=[](int32_t id) {
        return id==238||id==239||id==240||id==241||id==245||id==246||id==247||id==248;
    };
    float duration=retailAnimationDuration(state.association,59);
    if(!std::isfinite(duration)||duration<=0) {
        state.ready=false;error="Native drive-by association has no decoded duration.";return false;
    }
    bool beginning=state.association==235||state.association==242;
    float elapsed=std::max(0.0f,seconds-state.startedAt);
    if(beginning&&elapsed>=duration) {
        state.association=left?243:236;state.ready=true;state.startedAt=seconds;
        blend.poseTransition.blendRate=4;beginning=false;
    } else if(moving(state.association)&&elapsed*.6f>=duration) {
        switch(state.association) {
            case 238:case 240:state.association=237;break;
            case 239:case 241:state.association=236;break;
            case 245:case 247:state.association=244;break;
            case 246:case 248:state.association=243;break;
            default:break;
        }
        state.startedAt=seconds;blend.poseTransition.blendRate=1000;
    }
    const float magnitude=std::hypot(localTarget.x,localTarget.y,localTarget.z);
    const float directionX=magnitude>0?localTarget.x/magnitude:1;
    const float directionY=magnitude>0?localTarget.y/magnitude:0;
    if(!beginning) {
        const bool targetLeft=directionX<0,front=directionY>=0;
        if(state.targetLeft!=targetLeft) {
            state.association=left?(targetLeft?(front?248:246):(front?247:245)):
                targetLeft?(front?240:238):(front?241:239);
            state.startedAt=seconds;blend.poseTransition.blendRate=1000;
        }
        state.targetLeft=targetLeft;state.front=front;
    }
    duration=retailAnimationDuration(state.association,59);
    const uint32_t selected=groups.resolve(59,uint32_t(state.association),animation.clips());
    if(selected==UINT32_MAX||!std::isfinite(duration)||duration<=0||
        !animation.setActiveClipIndex(selected)||!animation.hasDecodedMotion()) {
        state.ready=false;error="Native drive-by association is unavailable.";return false;
    }
    elapsed=std::max(0.0f,seconds-state.startedAt);
    float sampleTime=beginning?elapsed:moving(state.association)?elapsed*.6f:0;
    if(!beginning&&!moving(state.association)) {
        const float lower=state.targetLeft?-1.2f:-1.4f;
        const float upper=state.targetLeft?1.2f:1.1f;
        sampleTime=duration*(1-(std::clamp(directionY,lower,upper)-lower)/(upper-lower));
    }
    sampleTime=std::clamp(sampleTime,0.0f,std::max(0.0f,duration-.001f));
    blend.poseTransition.key=(uint64_t(9)<<60)|(uint64_t(59)<<28)|uint32_t(state.association);
    blend.poseTransition.seconds=seconds;
    const std::vector<StorylandModelPoint> noGeometry;
    const auto& sourcePoints=skinGeometry?model.previewPoints():noGeometry;
    std::vector<StorylandModelPoint> bones;
    if(!storylandSampleModelAnimationLayered(model,animation,sampleTime,animation,sampleTime,0,
        nullptr,0,0,true,sourcePoints,points,bones,error,&blend.boneFrames,&blend.poseTransition)) {
        state.ready=false;return false;
    }
    const auto& armature=model.armatureBones();
    const auto pelvis=std::find_if(armature.begin(),armature.end(),
        [](const auto& bone){return bone.boneId==1;});
    if(pelvis==armature.end()||size_t(pelvis-armature.begin())>=bones.size()) {
        state.ready=false;error="Native passenger pose has no decoded pelvis.";return false;
    }
    const auto& anchor=bones[size_t(pelvis-armature.begin())];
    for(auto& point:points) {point.x-=anchor.x;point.y-=anchor.y;point.z-=anchor.z;}
    for(auto& frame:blend.boneFrames) {
        frame[12]-=anchor.x;frame[13]-=anchor.y;frame[14]-=anchor.z;
    }
    return true;
}

bool StorylandPlayTestAnimation::hasUpperBodyAction() const { return upperAction != UINT32_MAX; }
bool StorylandPlayTestAnimation::hasDeathAnimation() const { return lastDeathClipSampled; }

float StorylandPlayTestAnimation::locomotionStride(StorylandPlayTestMotion motion) const {
    if(motion==StorylandPlayTestMotion::Sprint)return sprintStride;
    return motion == StorylandPlayTestMotion::Run ? runStride : walkStride;
}

float StorylandPlayTestAnimation::locomotionStrideAtSpeed(float speed) const {
    // Match the walk/run/sprint blend continuously. Selecting a sprint stride
    // from the input state made cadence jump while speed was still ramping.
    const float walkSpeed=locomotionSpeed(StorylandPlayTestMotion::Walk);
    const float runSpeed=std::max(walkSpeed+.01f,locomotionSpeed(StorylandPlayTestMotion::Run));
    const float sprintSpeed=std::max(runSpeed+.01f,locomotionSpeed(StorylandPlayTestMotion::Sprint));
    if(!std::isfinite(speed)||speed<=walkSpeed)return walkStride;
    if(speed<=runSpeed)return walkStride+(runStride-walkStride)*std::clamp((speed-walkSpeed)/(runSpeed-walkSpeed),0.0f,1.0f);
    return runStride+(sprintStride-runStride)*std::clamp((speed-runSpeed)/(sprintSpeed-runSpeed),0.0f,1.0f);
}

float StorylandPlayTestAnimation::locomotionSpeed(StorylandPlayTestMotion motion) const {
    if(motion==StorylandPlayTestMotion::Sprint)return sprintStride/sprintDuration;
    return motion==StorylandPlayTestMotion::Run?runStride/runDuration:walkStride/walkDuration;
}

float StorylandPlayTestAnimation::locomotionDuration(StorylandPlayTestMotion motion) const {
    return motion == StorylandPlayTestMotion::Sprint ? sprintDuration :
        motion == StorylandPlayTestMotion::Run ? runDuration : walkDuration;
}

std::string StorylandPlayTestAnimation::clipName(StorylandPlayTestMotion motion) const {
    const uint32_t clip=motion==StorylandPlayTestMotion::Idle?idle:motion==StorylandPlayTestMotion::Sprint?sprint:(motion==StorylandPlayTestMotion::Run?run:walk);
    return clip<animation.clips().size()?animation.clips()[clip].name:std::string{};
}

bool StorylandPlayTestAnimation::sample(const StorylandModelFile& model, StorylandPlayTestMotion motion,
    float seconds, std::vector<StorylandModelPoint>& points, std::string& error) {
    const uint32_t clip=motion==StorylandPlayTestMotion::Idle?idle:
        motion==StorylandPlayTestMotion::Sprint?sprint:
        (motion==StorylandPlayTestMotion::Run?run:walk);
    if (clip>=animation.clips().size() || !std::isfinite(seconds) || !animation.setActiveClipIndex(clip)) {
        error="Locomotion animation is unavailable."; return false;
    }
    const float duration=animation.durationSeconds();
    const float time=duration>0?std::fmod(std::max(0.0f,seconds),duration):0;
    std::vector<StorylandModelPoint> bones;
    return storylandSampleModelAnimation(model,animation,time,true,model.previewPoints(),points,bones,error);
}

bool StorylandPlayTestAnimation::sampleBlended(const StorylandModelFile& model,
    StorylandPlayTestMotion motion, float seconds, StorylandAnimationBlendState& state,
    std::vector<StorylandModelPoint>& points, std::string& error) {
    if (!std::isfinite(seconds)) { error="Invalid animation time."; return false; }
    if (!state.initialized) {
        state.from=motion;
        state.to=motion;
        state.startedAt=seconds;
        state.initialized=true;
    }
    if (motion!=state.to) {
        state.from=state.to;
        state.to=motion;
        state.startedAt=seconds;
    }
    const float blend=std::clamp((seconds-state.startedAt)/std::max(0.01f,state.duration),0.0f,1.0f);
    if (state.from==state.to || blend>=1.0f) {
        state.from=state.to;
        return sample(model,state.to,seconds,points,error);
    }
    const auto index=[&](StorylandPlayTestMotion selected) {
        return selected==StorylandPlayTestMotion::Idle?idle:
            !groups.vcsLayout&&selected==StorylandPlayTestMotion::Sprint?sprint:
            (selected==StorylandPlayTestMotion::Run?run:walk);
    };
    if (!animation.setActiveClipIndex(index(state.from)) ||
        !secondaryAnimation.setActiveClipIndex(index(state.to))) {
        error="Could not select the source or destination animation clip.";
        return false;
    }
    const float firstDuration=animation.durationSeconds();
    const float secondDuration=secondaryAnimation.durationSeconds();
    const float firstTime=firstDuration>0?std::fmod(std::max(0.0f,seconds),firstDuration):0;
    const float secondTime=secondDuration>0?std::fmod(std::max(0.0f,seconds),secondDuration):0;
    std::vector<StorylandModelPoint> bones;
    if(!groups.vcsLayout)return storylandSampleLcsAnimationAssociations(model,
        {{&secondaryAnimation,secondTime,blend,false},{&animation,firstTime,1.0f-blend,false}},true,
        model.previewPoints(),points,bones,error);
    return storylandSampleModelAnimationBlended(model,animation,firstTime,
        secondaryAnimation,secondTime,blend,true,model.previewPoints(),points,bones,error);
}

std::string StorylandPlayTestAnimation::runStopClipName(unsigned variant) const {
    if(variant>=2)return {};
    const uint32_t clip=groups.resolve(0,(groups.vcsLayout?8u:5u)+variant,animation.clips());
    return clip<animation.clips().size()?animation.clips()[clip].name:std::string{};
}
float StorylandPlayTestAnimation::runStopDuration(unsigned variant) const {
    if(variant>=2)return 0;
    const uint32_t clip=groups.resolve(0,(groups.vcsLayout?8u:5u)+variant,animation.clips());
    return clip<animation.clips().size()?std::max(0.0f,animation.clips()[clip].duration):0;
}
float StorylandPlayTestAnimation::runStopDistance(unsigned variant,float fromSeconds,float toSeconds) const {
    if(variant>=2||!std::isfinite(fromSeconds)||!std::isfinite(toSeconds))return 0;
    const auto& keys=runStopRootKeys[variant];
    if(keys.size()<2)return 0;
    const float x=keys.back().tx-keys.front().tx,y=keys.back().ty-keys.front().ty;
    const float length=std::hypot(x,y);
    if(!std::isfinite(length)||length<1e-6f)return 0;
    const auto position=[&](float seconds) {
        seconds=std::clamp(seconds,keys.front().time,keys.back().time);
        auto upper=std::upper_bound(keys.begin(),keys.end(),seconds,
            [](float time,const StorylandAnimKey& key){return time<key.time;});
        if(upper==keys.begin())return std::array<float,2>{upper->tx,upper->ty};
        if(upper==keys.end())return std::array<float,2>{keys.back().tx,keys.back().ty};
        const auto& a=*(upper-1);const auto& b=*upper;
        const float amount=b.time>a.time?(seconds-a.time)/(b.time-a.time):0;
        return std::array<float,2>{a.tx+(b.tx-a.tx)*amount,a.ty+(b.ty-a.ty)*amount};
    };
    const auto from=position(fromSeconds),to=position(toSeconds);
    // The native clip is sampled without inventing a braking curve. Project
    // its authored horizontal trajectory onto this actor's facing direction.
    return ((to[0]-from[0])*x+(to[1]-from[1])*y)/length;
}

StorylandNativePoint StorylandPlayTestAnimation::retailAnimationRootDelta(const StorylandModelFile& model,
    int32_t animationId,int32_t group,float fromSeconds,float toSeconds) {
    if(animationId<0||!std::isfinite(fromSeconds)||!std::isfinite(toSeconds))return {};
    const uint32_t clip=group<0?uint32_t(animationId):
        groups.resolve(uint32_t(group),uint32_t(animationId),secondaryAnimation.clips());
    if(clip==UINT32_MAX||!secondaryAnimation.setActiveClipIndex(clip)||!secondaryAnimation.hasDecodedMotion())return {};
    const auto* association=group>=0?groups.find(uint32_t(group),uint32_t(animationId)):nullptr;
    const bool mirrored=association&&(association->flags&0x8000u);
    size_t pelvis=SIZE_MAX;
    for(size_t i=0;i<model.armatureBones().size();++i)
        if(model.armatureBones()[i].boneId==1) {pelvis=i;break;}
    if(pelvis==SIZE_MAX)return {};
    std::vector<StorylandModelPoint> unused,first,last;
    std::string error;
    const auto sample=[&](float time,std::vector<StorylandModelPoint>& bones) {
        const float elapsed=std::clamp(time,0.0f,std::max(0.0f,secondaryAnimation.durationSeconds()));
        return mirrored?storylandSampleMirroredModelAnimation(model,secondaryAnimation,elapsed,false,{},unused,bones,error):
            storylandSampleModelAnimation(model,secondaryAnimation,elapsed,false,{},unused,bones,error);
    };
    if(!sample(fromSeconds,first)||!sample(toSeconds,last)||pelvis>=first.size()||pelvis>=last.size())return {};
    const float forward=storylandPedModelForwardAngle(model)*.01745329251994329577f;
    const float x=last[pelvis].x-first[pelvis].x,y=last[pelvis].y-first[pelvis].y;
    return {x*std::cos(forward)+y*std::sin(forward),x*std::sin(forward)-y*std::cos(forward),
        last[pelvis].z-first[pelvis].z};
}

StorylandNativePoint StorylandPlayTestAnimation::retailAnimationExtractedRootDelta(
    int32_t animationId,int32_t group,float fromSeconds,float toSeconds) {
    if(animationId<0||group<0||!std::isfinite(fromSeconds)||!std::isfinite(toSeconds))return {};
    const auto* association=groups.find(uint32_t(group),uint32_t(animationId));
    if(!association)return {};
    const uint32_t clip=groups.resolve(uint32_t(group),uint32_t(animationId),secondaryAnimation.clips());
    if(clip==UINT32_MAX||!secondaryAnimation.setActiveClipIndex(clip)||!secondaryAnimation.hasDecodedMotion())return {};
    const auto native=storylandNativeAnimationRootDelta(secondaryAnimation,association->flags,
        fromSeconds,toSeconds,groups.vcsLayout&&(association->flags&0x8000u));
    // Native animation +Y is forward; runtime motion is forward/right.
    return {native[1],native[0],0};
}

float StorylandPlayTestAnimation::retailAnimationContactTime(const StorylandModelFile& model,
    int32_t animationId,int32_t group,uint32_t boneId) {
    if(animationId<0)return -1;
    const uint32_t clip=group<0?uint32_t(animationId):
        groups.resolve(uint32_t(group),uint32_t(animationId),upperAnimation.clips());
    if(clip==UINT32_MAX||!upperAnimation.setActiveClipIndex(clip)||!upperAnimation.hasDecodedMotion())return -1;
    const auto& skeleton=model.armatureBones();
    size_t contact=SIZE_MAX,chest=SIZE_MAX;
    for(size_t i=0;i<skeleton.size();++i) {
        if(skeleton[i].boneId==boneId)contact=i;
        if(skeleton[i].boneId==3)chest=i;
    }
    if(contact==SIZE_MAX||chest==SIZE_MAX)return -1;
    const auto* association=group>=0?groups.find(uint32_t(group),uint32_t(animationId)):nullptr;
    const bool mirrored=association&&(association->flags&0x8000u);
    const float duration=upperAnimation.durationSeconds();
    if(!std::isfinite(duration)||duration<=0)return -1;
    const float facing=storylandPedModelForwardAngle(model)*.01745329251994329577f;
    const int samples=std::clamp(int(std::ceil(duration*30)),3,150);
    float best=-INFINITY,time=-1;
    std::vector<StorylandModelPoint> points,bones;
    std::vector<std::array<float,16>> frames;
    std::string error;
    for(int i=1;i<samples;++i) {
        const float elapsed=duration*float(i)/samples;
        const bool ready=mirrored?
            storylandSampleMirroredModelAnimation(model,upperAnimation,elapsed,true,{},points,bones,error,&frames):
            storylandSampleModelAnimationLayered(model,upperAnimation,elapsed,upperAnimation,elapsed,0,
                nullptr,0,0,true,{},points,bones,error,&frames);
        if(!ready||frames.size()!=skeleton.size())continue;
        const float extension=(frames[contact][12]-frames[chest][12])*std::cos(facing)+
            (frames[contact][13]-frames[chest][13])*std::sin(facing);
        if(extension>best) {best=extension;time=elapsed;}
    }
    return time;
}

float StorylandPlayTestAnimation::movementActionDuration(StorylandPlayTestMotion motion) const {
    const char* name=motion==StorylandPlayTestMotion::Jump?"jumplaunch":
        motion==StorylandPlayTestMotion::Land?"jumpland":motion==StorylandPlayTestMotion::Fall?"jumpglide":
        motion==StorylandPlayTestMotion::Swim?"swim":motion==StorylandPlayTestMotion::SwimFast?"swimfast":nullptr;
    if(!name)return 0;
    const bool swimming=motion==StorylandPlayTestMotion::Swim||motion==StorylandPlayTestMotion::SwimFast;
    for(const auto& clip:animation.clips())
        if((clipMatches(clip.bankName,"ped")||(swimming&&clipMatches(clip.bankName,"swim")))&&
            clipKeyMatches(clip.name,name)&&
            std::isfinite(clip.duration)&&clip.duration>0)return clip.duration;
    return 0;
}

bool StorylandPlayTestAnimation::sampleMovementAction(const StorylandModelFile& model,
    StorylandPlayTestMotion motion, float seconds, std::vector<StorylandModelPoint>& points,
    std::string& error,float locomotionPhase,std::vector<std::array<float,16>>* frames,bool skinGeometry,
    StorylandAnimationTransition* transition,float presentationSeconds) {
    if(groups.vcsLayout&&motion==StorylandPlayTestMotion::Jump) {
        const float launch=movementActionDuration(StorylandPlayTestMotion::Jump);
        const bool launching=launch>0&&seconds<launch;
        return sampleRetailAnimationId(model,launching?136:137,
            launching?seconds:std::max(0.0f,seconds-launch),points,error,0,frames,
            StorylandPlayTestMotion::Idle,-1,1,skinGeometry,transition,8);
    }
    const std::vector<StorylandModelPoint> noGeometry;
    const auto& sourcePoints=skinGeometry?model.previewPoints():noGeometry;
    if(motion==StorylandPlayTestMotion::Dead)lastDeathClipSampled=false;
    if(transition)transition->seconds=presentationSeconds>=0?presentationSeconds:seconds;
    std::vector<std::string> names;
    switch (motion) {
    case StorylandPlayTestMotion::Sprint: names=sprint!=UINT32_MAX?std::vector<std::string>{animation.clips()[sprint].name}:std::vector<std::string>{"sprint_civi","sprint_panic"}; break;
    case StorylandPlayTestMotion::Jump: {
        const float launch=movementActionDuration(StorylandPlayTestMotion::Jump);
        if(launch>0&&seconds>=launch) {names={"jump_glide","fall_glide"};seconds-=launch;}
        else names={"jump_launch","jump_glide"};
        break;
    }
    // FALL_fall is the native head-first tumble (association 139), not the
    // ordinary airborne pose used while stepping down or leaving a vehicle.
    case StorylandPlayTestMotion::Fall: names={"fall_glide","jump_glide"}; break;
    case StorylandPlayTestMotion::Land: names={"jump_land","fall_land","fall_collapse"}; break;
    case StorylandPlayTestMotion::SwimIdle: names={"swim_idle"}; break;
    case StorylandPlayTestMotion::Swim: names={"swim"}; break;
    case StorylandPlayTestMotion::SwimFast: names={"swim_fast"}; break;
    case StorylandPlayTestMotion::Drown: names={"drown"}; break;
    case StorylandPlayTestMotion::Cough: names={"gas_cwr","gas_cower","cower"}; break;
    case StorylandPlayTestMotion::Dead: names={"ko_shot_front","ko_shot_face","ko_shot_stom","ko_shot_arml","ko_shot_armr","ko_shot_legl","ko_shot_legr","ko_skid_front","ko_spin_r","ko_spin_l","fall_collapse","death","floor_hit","die_front","die_back"}; break;
    default: return false;
    }
    const bool waterMotion=motion==StorylandPlayTestMotion::SwimIdle||motion==StorylandPlayTestMotion::Swim||
        motion==StorylandPlayTestMotion::SwimFast||motion==StorylandPlayTestMotion::Drown;
    for (const auto& name : names) {
        const auto normalizedName=clipKey(name);
        for (const auto& clip : animation.clips()) {
            if(!clipMatches(clip.bankName,"ped")&&!(waterMotion&&clipMatches(clip.bankName,"swim")))continue;
            if (motion==StorylandPlayTestMotion::Sprint&&sprint!=UINT32_MAX&&clip.index!=sprint)continue;
            if (!clipKeyMatches(clip.name,normalizedName) || !animation.setActiveClipIndex(clip.index) || !animation.hasDecodedMotion()) continue;
            if(transition)transition->key=(uint64_t(4)<<60)|clip.index;
            const float duration=std::max(0.001f,animation.durationSeconds());
            const float elapsed=std::max(0.0f,seconds);
            const bool repeat=motion==StorylandPlayTestMotion::Sprint||motion==StorylandPlayTestMotion::Fall||
                (motion==StorylandPlayTestMotion::Jump&&normalizedName!="jumplaunch")||
                (waterMotion&&(groups.vcsLayout||motion!=StorylandPlayTestMotion::Drown));
            const float time=repeat?
                (locomotionPhase>=0?std::fmod(locomotionPhase,1.0f)*duration:std::fmod(elapsed,duration)) :
                std::min(elapsed,groups.vcsLayout?duration-0.001f:duration);
            std::vector<StorylandModelPoint> bones;
            if(storylandSampleModelAnimationLayered(model,animation,time,animation,time,0,nullptr,0,0,true,sourcePoints,points,bones,error,frames,transition)) {
                if(motion==StorylandPlayTestMotion::Dead)lastDeathClipSampled=true;
                return true;
            }
        }
    }
    if(motion==StorylandPlayTestMotion::Dead) {
        for(const auto& clip:animation.clips()) {
            if(!clipMatches(clip.bankName,"ped")||!isDeathClip(clip.name)||!animation.setActiveClipIndex(clip.index)||!animation.hasDecodedMotion())continue;
            const float duration=std::max(0.001f,animation.durationSeconds());
            const float time=std::min(std::max(0.0f,seconds),groups.vcsLayout?duration-0.001f:duration);
            std::vector<StorylandModelPoint> bones;
            if(storylandSampleModelAnimationLayered(model,animation,time,animation,time,0,nullptr,0,0,true,sourcePoints,points,bones,error,frames,transition)) {
                lastDeathClipSampled=true;
                return true;
            }
        }
    }
    // A missing optional clip must not stop the player model from rendering.
    const uint32_t fallback=motion==StorylandPlayTestMotion::Sprint?run:idle;
    if(fallback==UINT32_MAX||!animation.setActiveClipIndex(fallback))return false;
    std::vector<StorylandModelPoint> bones;
    const float duration=animation.durationSeconds();
    const float time=duration>0?std::fmod(std::max(0.0f,seconds),duration):0;
    return storylandSampleModelAnimationLayered(model,animation,time,animation,time,0,nullptr,0,0,true,
        sourcePoints,points,bones,error,frames,transition);
}

bool StorylandPlayTestAnimation::sampleTree(const StorylandModelFile& model,
    StorylandPlayTestMotion motion,float seconds,StorylandAnimationBlendState& state,
    std::vector<StorylandModelPoint>& points,std::string& error,bool skinGeometry) {
    const std::vector<StorylandModelPoint> noGeometry;
    const auto& sourcePoints=skinGeometry?model.previewPoints():noGeometry;
    if(!std::isfinite(seconds)||!std::isfinite(state.measuredSpeed)||!std::isfinite(state.speed)||
        !std::isfinite(state.locomotionPhase)||!std::isfinite(state.walkSpeed)||state.walkSpeed<=0||
        !std::isfinite(state.runSpeed)||state.runSpeed<=0||!std::isfinite(state.weaponAnimationTime)||
        !std::isfinite(state.weaponAnimationWeight)||state.weaponAnimationWeight<0||state.weaponAnimationWeight>1||
        !std::isfinite(state.movementForward)||!std::isfinite(state.movementRight)||
        !std::isfinite(state.weaponLocomotionForward)||!std::isfinite(state.weaponLocomotionRight)||
        !std::isfinite(state.weaponAimBlend)||!std::isfinite(state.weaponAimTime)||
        !std::isfinite(state.nativeMoveRatio)||!std::isfinite(state.nativeSprintWeight)||
        !std::isfinite(state.nativeStopTime)||!std::isfinite(state.sprintAssociationWeight)) {
        error="Animation time, movement or weapon association weight is invalid.";return false;
    }
    const bool nativeWeaponTime=!groups.vcsLayout&&state.weaponAnimationTime>=0&&motion!=StorylandPlayTestMotion::Sprint;
    if(!state.initialized) {
        state.initialized=true;
        state.from=state.to=StorylandPlayTestMotion::Idle;
        state.lastSampleTime=seconds;
        state.speed=0;
    }
    state.poseTransition.key=(uint64_t(1)<<60)|
        (motion<=StorylandPlayTestMotion::Sprint?0u:uint32_t(motion));
    state.poseTransition.seconds=seconds;
    state.poseTransition.blendRate=8;
    const StorylandPlayTestMotion previous=state.to;
    const auto movingSwim=[](StorylandPlayTestMotion value) {
        return value==StorylandPlayTestMotion::Swim||value==StorylandPlayTestMotion::SwimFast;
    };
    if(previous!=motion) {
        float inheritedTime=0;
        if(groups.vcsLayout&&movingSwim(previous)&&movingSwim(motion)&&
            seconds>=state.lastSampleTime&&seconds-state.lastSampleTime<=.1f) {
            const float previousDuration=movementActionDuration(previous);
            const float duration=movementActionDuration(motion);
            if(previousDuration>0&&duration>0) {
                // VCS AddAnimation/SyncAnimation (0x225028/0x18EC48) transfers
                // normalized movement time; changing sprint input is not Start(0).
                const float phase=std::fmod(std::max(0.0f,seconds-state.actionStartedAt)/previousDuration,1.0f);
                inheritedTime=phase*duration;
            }
        }
        state.actionStartedAt=seconds-inheritedTime;
    }
    state.to=motion;
    const float delta=std::clamp(seconds-state.lastSampleTime,0.0f,0.1f);
    state.lastSampleTime=seconds;
    if(motion==StorylandPlayTestMotion::Jump||
        motion==StorylandPlayTestMotion::Fall||motion==StorylandPlayTestMotion::Land||
        motion==StorylandPlayTestMotion::Cough||motion==StorylandPlayTestMotion::Dead||
        motion==StorylandPlayTestMotion::SwimIdle||motion==StorylandPlayTestMotion::Swim||
        motion==StorylandPlayTestMotion::SwimFast||motion==StorylandPlayTestMotion::Drown) {
        state.weaponAimBlend=state.weaponAimTime=0;
        state.weaponAimWasActive=false;
        state.boneFrames.clear();
        return sampleMovementAction(model,motion,seconds-state.actionStartedAt,points,error,
            motion==StorylandPlayTestMotion::Sprint&&state.distanceDrivenPhase?state.locomotionPhase:-1.0f,
            &state.boneFrames,skinGeometry,&state.poseTransition,
            movingSwim(motion)||motion==StorylandPlayTestMotion::SwimIdle||
                motion==StorylandPlayTestMotion::Drown?seconds:-1.0f);
    }
    const float sprintSpeed=std::max(state.runSpeed+.01f,locomotionSpeed(StorylandPlayTestMotion::Sprint));
    const float desired=state.measuredSpeed>=0.0f?std::clamp(state.measuredSpeed,0.0f,sprintSpeed):
        motion==StorylandPlayTestMotion::Idle?0.0f:motion==StorylandPlayTestMotion::Walk?state.walkSpeed:
        motion==StorylandPlayTestMotion::Sprint?sprintSpeed:state.runSpeed;
    if(motion==StorylandPlayTestMotion::Sprint) {
        state.weaponAimBlend=state.weaponAimTime=0;state.weaponAimWasActive=false;
        state.weaponAiming=false;state.upperActive=false;
    }
    state.speed=desired;
    const float inferredRatio=desired<=state.walkSpeed?desired/state.walkSpeed:
        1+(desired-state.walkSpeed)/std::max(.01f,state.runSpeed-state.walkSpeed);
    const float ratio=state.nativeMoveRatio>=0?state.nativeMoveRatio:std::clamp(inferredRatio,0.0f,2.0f);
    const bool sprinting=motion==StorylandPlayTestMotion::Sprint;
    const auto sampleStop=[&](unsigned variant,float elapsed) {
        const uint32_t clip=groups.resolve(0,(groups.vcsLayout?8u:5u)+variant,animation.clips());
        if(clip==UINT32_MAX||!animation.setActiveClipIndex(clip)||!animation.hasDecodedMotion())return false;
        const float time=std::clamp(elapsed,0.0f,std::max(0.0f,animation.durationSeconds()-.001f));
        state.gaitWeights={0,0,0,0};
        state.sprintAssociationWeight=0;state.sprintAssociationDelta=-1000;
        state.poseTransition.key=(uint64_t(5)<<60)|clip;
        state.poseTransition.seconds=seconds;state.poseTransition.blendRate=1000;
        std::vector<StorylandModelPoint> bones;
        return storylandSampleModelAnimationLayered(model,animation,time,animation,time,0,nullptr,0,0,true,
            sourcePoints,points,bones,error,&state.boneFrames,&state.poseTransition);
    };
    if(playerAnimation&&state.nativeStopVariant>=0&&state.nativeStopVariant<2) {
        state.runStopVariant=-1;
        if(sampleStop(unsigned(state.nativeStopVariant),state.nativeStopTime))return true;
    }
    if(playerAnimation&&state.runStopVariant<0) {
        const int variant=storylandNativeSprintStopVariant(state.sprintAssociationWeight,
            state.sprintAssociationDelta<0,ratio,state.locomotionPhase,sprinting);
        if(variant>=0&&runStopDuration(unsigned(variant))>0) {
            state.runStopVariant=variant;state.runStopStartedAt=seconds;
        }
    }
    if(state.runStopVariant>=0) {
        const float elapsed=std::max(0.0f,seconds-state.runStopStartedAt);
        if(ratio<.4f&&elapsed<runStopDuration(unsigned(state.runStopVariant))) {
            if(sampleStop(unsigned(state.runStopVariant),elapsed))return true;
        }
        state.runStopVariant=-1;
        state.gaitWeights={1,0,0,0};
    }
    float sprintDelta=sprinting?2.0f:-1.0f;
    if((!sprinting||ratio<.4f)&&state.sprintAssociationDelta<0&&state.sprintAssociationWeight<.8f&&ratio<1)
        sprintDelta=-8;
    float sprintWeight=state.nativeSprintWeight;
    if(sprintWeight<0)sprintWeight=std::clamp(state.sprintAssociationWeight+sprintDelta*delta,0.0f,1.0f);
    state.sprintAssociationWeight=sprintWeight;state.sprintAssociationDelta=sprintDelta;
    state.gaitWeights=storylandStepNativeGaitWeights(state.gaitWeights,unsigned(motion),
        playerAnimation,ratio,sprintWeight,delta);
    if(sprint==UINT32_MAX) {state.gaitWeights[2]+=state.gaitWeights[3];state.gaitWeights[3]=0;}
    if(!animation.setActiveClipIndex(idle)||!animation.hasDecodedMotion()) {
        error="The locomotion tree could not select its idle animation.";return false;
    }
    // Use authored root displacement for VCS as well as LCS. An idle actor has
    // no stride to advance, and a slower actor does not keep a full-speed walk.
    if(!state.distanceDrivenPhase) {
        const float stride=locomotionStrideAtSpeed(desired);
        if(stride>0&&desired>0)
            state.locomotionPhase=std::fmod(state.locomotionPhase+delta*desired/stride,1.0f);
    }
    const StorylandAnimFile* upper=nullptr;
    float upperTime=0,upperWeight=0;
    bool partial=!fullBodyAction;
    if(nativeWeaponTime&&upperAction!=UINT32_MAX) {
        upper=&upperAnimation;
        upperTime=std::clamp(state.weaponAnimationTime,0.0f,upperAnimation.durationSeconds());
        upperWeight=state.weaponAnimationWeight;
    } else {
        if(upperAction==UINT32_MAX)state.upperActive=false;
        bool action=state.upperActive;
        if(action) {
            const float elapsed=std::max(0.0f,seconds-state.upperStartedAt);
            const float duration=upperAnimation.durationSeconds();
            // The reload association plays at its authored speed. The weapon's
            // reload timer is not the animation duration (Uzi/Beretta differ).
            upperTime=state.weaponReloading?elapsed:elapsed+firePoseStart;
            const float end=state.weaponReloading?duration:firePoseEnd;
            if(!state.weaponReloading)state.weaponAimTime=aimPoseTime;
            if(end<=0||upperTime>=end) {
                state.upperActive=false;action=false;
            }
        }
        const bool aiming=state.weaponAiming&&aimAction!=UINT32_MAX;
        const bool requested=aiming||action;
        if(requested&&!state.weaponAimWasActive&&state.weaponAimBlend==0)state.weaponAimTime=0;
        state.weaponAimWasActive=requested;
        // Native association blend deltas are amounts per second, rather than
        // a mask inferred from bone names. Play the raise segment, then hold
        // at CWeaponInfo's loop start until the next actual shot.
        state.weaponAimBlend=std::clamp(state.weaponAimBlend+(requested?8.0f:-8.0f)*delta,0.0f,1.0f);
        if(aiming)state.weaponAimTime=std::min(aimPoseTime,state.weaponAimTime+delta);
        if(action)upper=&upperAnimation;
        else if(state.weaponAimBlend>0&&aimAction!=UINT32_MAX&&upperAnimation.setActiveClipIndex(aimAction)) {
            upper=&upperAnimation;upperTime=std::min(state.weaponAimTime,aimPoseTime);partial=true;
        }
        if(upper)upperWeight=state.weaponAimBlend;
    }
    const float movingGaitWeight=state.gaitWeights[1]+state.gaitWeights[2]+state.gaitWeights[3];
    const bool movingWeapon=state.allowAimedLocomotion&&weaponUsesMovingAim&&upper&&
        (desired>0.0001f||movingGaitWeight>0.0001f);
    const float movementLength=std::hypot(state.movementForward,state.movementRight);
    if(movementLength>0.0001f) {
        state.weaponLocomotionForward=state.movementForward/movementLength;
        state.weaponLocomotionRight=state.movementRight/movementLength;
    }
    if(movingWeapon) {
        partial=true;
        state.poseTransition.key|=uint64_t(1)<<59;
    }
    std::vector<StorylandModelPoint> bones;
    std::vector<StorylandAnimationPoseLayer> associations;
    const float baseWeight=upper&&!partial?1.0f-upperWeight:1.0f;
    if(upper&&upperWeight>0) {
        StorylandAnimationPoseLayer weaponLayer{upper,upperTime,upperWeight,partial};
        weaponLayer.upperBodyOnly=movingWeapon;
        associations.push_back(weaponLayer);
    }
    {
        const uint32_t clips[]{idle,walk,run,sprint};
        const uint32_t movementIds[]{groups.vcsLayout?6u:3u,groups.vcsLayout?2u:0u,
            groups.vcsLayout?4u:1u,groups.vcsLayout?5u:2u};
        std::array<int32_t,2> directionGroups{locomotionGroup,locomotionGroup};
        std::array<float,2> directionWeights{1,0};
        if(movingWeapon) {
            const float forward=state.weaponLocomotionForward,right=state.weaponLocomotionRight;
            const float total=std::abs(forward)+std::abs(right);
            if(total>0.0001f)directionWeights={std::abs(forward)/total,std::abs(right)/total};
            const auto directionGroup=[&](const char* direction) {
                const std::string name=std::string(weaponUsesRocketLocomotion?"walk_rocket_":"walk_")+direction;
                for(const auto& entry:groups.groups) {
                    const auto association=entry.second.associations.find(movementIds[1]);
                    if(association!=entry.second.associations.end()&&clipMatches(association->second.name,name))
                        return int32_t(entry.first);
                }
                return int32_t(-1);
            };
            if(directionWeights[0]>0&&forward<0)directionGroups[0]=directionGroup("back");
            if(directionWeights[1]>0)directionGroups[1]=directionGroup(right<0?"left":"right");
            for(size_t direction=0;direction<directionGroups.size();++direction) {
                if(directionWeights[direction]>0&&directionGroups[direction]<0) {
                    error="The native animation table lacks an aimed movement direction.";return false;
                }
            }
        }
        for(size_t index=0;index<4;++index) {
            const float weight=state.gaitWeights[index]*baseWeight;
            if(weight<=0||clips[index]==UINT32_MAX)continue;
            const size_t directionCount=index==0?1:directionGroups.size();
            for(size_t direction=0;direction<directionCount;++direction) {
                const float layerWeight=weight*(index==0?1:directionWeights[direction]);
                if(layerWeight<=0)continue;
                const uint32_t clip=index==0||directionGroups[direction]==locomotionGroup?clips[index]:
                    groups.resolve(uint32_t(directionGroups[direction]),movementIds[index],animation.clips());
                if(clip==UINT32_MAX) {
                    error="The native aimed movement association has no decoded clip.";return false;
                }
                auto tracks=locomotionTracks.find(clip);
                if(tracks==locomotionTracks.end()) {
                    if(!secondaryAnimation.setActiveClipIndex(clip)||!secondaryAnimation.hasDecodedMotion()) {
                        error="The native locomotion association has no decoded motion.";return false;
                    }
                    tracks=locomotionTracks.emplace(clip,secondaryAnimation.tracks()).first;
                }
                const float duration=std::max(.001f,animation.clips()[clip].duration);
                const float time=index==0?std::fmod(std::max(0.0f,seconds),duration):state.locomotionPhase*duration;
                associations.push_back({&animation,time,layerWeight,false,&tracks->second});
            }
        }
    }
    // Native thighs descend from spine (tag 2). Keep that shared hip frame
    // on the gait; torso (tag 3) and its children retain the weapon action.
    StorylandPedAimPose aim;
    aim.state=&state.weaponAimIk;aim.transition=&state.poseTransition;aim.yaw=state.weaponAimYaw;aim.pitch=state.weaponAimPitch;
    aim.seconds=delta;aim.arm=weaponUsesArmAim;aim.modelForwardAngle=storylandPedModelForwardAngle(model);
    aim.aiming=state.weaponAiming&&state.weaponAimDirectionValid&&!state.weaponReloading;
    aim.weight=!state.weaponReloading?state.weaponAimBlend:0;
    return storylandSampleNativeWeaponAssociations(model,associations,aim,
        sourcePoints,points,bones,error,&state.boneFrames);
}

StorylandPlayTestMovement storylandMovePlayTestPlayer(float x,float y,float heading,float cameraYaw,
    const StorylandPlayTestInput& input,float seconds) {
    StorylandPlayTestMovement result{x,y,heading,StorylandPlayTestMotion::Idle};
    if (!std::isfinite(seconds) || seconds<=0 || !std::isfinite(cameraYaw)) return result;
    const float forward=input.forwardAmount();
    const float left=-input.horizontal();
    const float magnitude=std::sqrt(forward*forward+left*left);
    if (magnitude==0) return result;
    constexpr float radians=0.01745329251994329577f;
    const float yaw=cameraYaw*radians;
    const float dx=(std::cos(yaw)*forward-std::sin(yaw)*left)/magnitude;
    const float dy=(std::sin(yaw)*forward+std::cos(yaw)*left)/magnitude;
    const float distance=(input.sprinting?5.5f:input.running?3.8f:1.25f)*input.movementAmount()*std::min(seconds,0.1f);
    result.x+=dx*distance; result.y+=dy*distance;
    result.heading=std::atan2(dy,dx)/radians;
    result.motion=input.sprinting?StorylandPlayTestMotion::Sprint:input.running?StorylandPlayTestMotion::Run:StorylandPlayTestMotion::Walk;
    return result;
}

bool storylandLoadPlayTestPedestrian(const StorylandDtzArchive& archive, const std::string& playerName,
    StorylandModelFile& model, std::vector<RgbaImage>& images, std::vector<std::string>& textureNames,
    std::string& modelName, std::string& error) {
    images.clear(); textureNames.clear(); modelName.clear();
    const auto normalize=[](std::string name) {
        name=clipLower(name);
        const auto slash=name.find_last_of("/\\"); if(slash!=std::string::npos) name.erase(0,slash+1);
        const auto dot=name.find_last_of('.'); if(dot!=std::string::npos) name.resize(dot);
        return name;
    };
    const auto& entries=archive.dirEntries();
    for (size_t index=0;index<entries.size();++index) {
        const auto& entry=entries[index];
        if (entry.detectedExtension!=".mdl" || entry.name==playerName) continue;
        std::vector<uint8_t> bytes;
        StorylandModelFile candidate;
        if (!archive.extractDirEntryBytes(index,bytes,error) ||
            !candidate.loadFromMemory(bytes,std::wstring(entry.name.begin(),entry.name.end()),error)) {
            if (fatalPs2PacketError(error)) return false;
            continue;
        }
        if (candidate.modelKind()!=StorylandModelKind::PedModel || candidate.armatureBones().empty() ||
            candidate.previewPoints().empty() || candidate.previewTriangles().empty()) continue;
        std::vector<std::string> required;
        for (const auto& name:candidate.previewMaterialTextureNames()) if(!name.empty()) required.push_back(normalize(name));
        for (const auto& name:candidate.previewMaterialEnvironmentNames()) if(!name.empty()) required.push_back(normalize(name));
        for (const auto& name:candidate.textureNameHints()) if(!name.empty()) required.push_back(normalize(name));
        for (size_t textureIndex=0;textureIndex<entries.size();++textureIndex) {
            const auto& textureEntry=entries[textureIndex];
            if (textureEntry.detectedExtension!=".chk" && textureEntry.detectedExtension!=".xtx") continue;
            LeedsTextureArchive textures;
            if (!archive.extractDirEntryBytes(textureIndex,bytes,error) ||
                !textures.loadFromMemory(bytes,LeedsPlatform::Auto,error,std::wstring(textureEntry.name.begin(),textureEntry.name.end()))) continue;
            bool matches=false;
            for (const auto& raster:textures.textures())
                if (std::find(required.begin(),required.end(),normalize(raster.name))!=required.end()) matches=true;
            if (!matches && !(required.empty() && normalize(entry.name)==normalize(textureEntry.name))) continue;
            for (size_t raster=0;raster<textures.textures().size();++raster) {
                RgbaImage image;
                if (!textures.decodeTexture(raster,image,error) || image.rgba.empty()) continue;
                textureNames.push_back(clipLower(textures.textures()[raster].name));
                images.push_back(std::move(image));
            }
            if (images.empty()) continue;
            model=std::move(candidate);modelName=entry.name;error.clear();return true;
        }
    }
    error="No other readable pedestrian model with matching textures was found.";return false;
}

float storylandPedModelForwardAngle(const StorylandModelFile& model) {
    float nativeForward=90.0f;
    if(storylandNativeAnimationForwardAngle(model,nativeForward))return nativeForward;
    const StorylandModelBone* left = nullptr;
    const StorylandModelBone* right = nullptr;
    auto normalize = [](std::string name) {
        name = clipLower(name);
        const auto prefix = name.find("bip01_");
        if (prefix != std::string::npos) name.erase(prefix, 6);
        return name;
    };
    for (const auto& bone : model.armatureBones()) {
        if (!bone.hasPreviewPosition) continue;
        const auto name = normalize(bone.name);
        if ((name == "l_clavicle" || name == "l_upperarm") && !left) left = &bone;
        if ((name == "r_clavicle" || name == "r_upperarm") && !right) right = &bone;
    }
    if (left && right) {
        const float sideX = right->previewPosition.x - left->previewPosition.x;
        const float sideY = right->previewPosition.y - left->previewPosition.y;
        if (std::isfinite(sideX) && std::isfinite(sideY) && std::hypot(sideX, sideY) > 0.001f)
            return std::atan2(sideX, -sideY) * 57.295779513f;
    }
    return 90.0f;
}

StorylandPlayTestCamera storylandMovePlayTestCamera(float yaw, float pitch, float playerHeading,
    const StorylandPlayTestCameraInput& input, float seconds) {
    StorylandPlayTestCamera result{yaw, pitch};
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(seconds) || seconds <= 0)
        return result;
    const float dt = std::min(seconds, 0.1f);
    const float horizontal = float(input.right) - float(input.left);
    const float vertical = float(input.up) - float(input.down);
    result.yaw -= horizontal * 105.0f * dt;
    result.pitch -= vertical * 63.0f * dt;
    if (std::isfinite(input.yawDelta)) result.yaw += std::clamp(input.yawDelta, -360.0f, 360.0f);
    if (std::isfinite(input.pitchDelta)) result.pitch += std::clamp(input.pitchDelta, -180.0f, 180.0f);
    if (input.refocus && horizontal == 0 && vertical == 0 &&
        input.yawDelta == 0 && input.pitchDelta == 0 && std::isfinite(playerHeading)) {
        const float blend = 1.0f - std::exp(-10.0f * dt);
        result.yaw += std::remainder(playerHeading - result.yaw, 360.0f) * blend;
        result.pitch += (15.0f - result.pitch) * blend;
    }
    result.yaw = std::remainder(result.yaw, 360.0f);
    result.pitch = std::max(-45.0f, std::min(89.5f, result.pitch));
    return result;
}

namespace {
std::string populationTextureName(std::string name) {
    name=clipLower(name);
    const auto slash=name.find_last_of("/\\");
    if(slash!=std::string::npos) name.erase(0,slash+1);
    const auto dot=name.find_last_of('.');
    if(dot!=std::string::npos) name.resize(dot);
    return name;
}
uint64_t populationGeometryIdentity(const StorylandModelFile& model) {
    uint64_t hash=1469598103934665603ull;
    const auto add=[&](const void* data,size_t size) {
        const auto* bytes=static_cast<const uint8_t*>(data);
        for(size_t index=0;index<size;++index) { hash^=bytes[index]; hash*=1099511628211ull; }
    };
    for(const auto& point:model.previewPoints()) {
        add(&point.x,sizeof(point.x));add(&point.y,sizeof(point.y));add(&point.z,sizeof(point.z));
    }
    for(const auto& triangle:model.previewTriangles()) {
        add(&triangle.a,sizeof(triangle.a));add(&triangle.b,sizeof(triangle.b));add(&triangle.c,sizeof(triangle.c));
    }
    return hash;
}
}

static void storylandLoadPlayTestAssetTextures(const StorylandDtzArchive& archive,
    std::vector<StorylandPlayTestAsset>& candidates,std::string& error) {
    const auto& entries=archive.dirEntries();
    constexpr uint64_t maximumTextureArchiveBytes=16u*1024u*1024u;
    std::map<std::string,std::vector<size_t>> required;
    std::map<std::string,std::vector<size_t>> modelStems;
    for(size_t index=0;index<candidates.size();++index) {
        modelStems[populationTextureName(candidates[index].name)].push_back(index);
        const auto& model=candidates[index].model;
        for(size_t material=0;material<model.previewMaterialTextureNames().size();++material) {
            const auto name=storylandMaterialTextureName(model,material);
            if(!name.empty()&&name.front()!='<') required[populationTextureName(name)].push_back(index);
        }
        for(const auto& name:candidates[index].model.previewMaterialEnvironmentNames())
            if(!name.empty()) required[populationTextureName(name)].push_back(index);
        for(const auto& name:candidates[index].model.textureNameHints())
            if(!name.empty()&&name.front()!='<') required[populationTextureName(name)].push_back(index);
    }
    std::vector<size_t> textureBytes(candidates.size(),0);
    std::vector<std::map<std::string,unsigned>> texturePriority(candidates.size());
    constexpr size_t maximumModelTextureBytes=24u*1024u*1024u;
    constexpr size_t maximumPopulationTextureBytes=256u*1024u*1024u;
    size_t totalTextureBytes=0;
    const auto assignRaster=[&](const std::string& sourceName,const std::string& rasterName,
        const RgbaImage& image) {
        if(image.width<=0||image.height<=0||
            image.rgba.size()!=size_t(image.width)*size_t(image.height)*4u)return;
        const size_t imageBytes=rgbaImageDecodedBytes(image);
        const std::string name=populationTextureName(rasterName);
        if(name.empty())return;
        std::set<size_t> recipients;
        const auto companion=modelStems.find(populationTextureName(sourceName));
        if(companion!=modelStems.end())recipients.insert(companion->second.begin(),companion->second.end());
        const auto named=required.find(name);
        if(named!=required.end())recipients.insert(named->second.begin(),named->second.end());
        for(const size_t assetIndex:recipients) {
            auto& asset=candidates[assetIndex];
            const bool owner=companion!=modelStems.end()&&
                std::find(companion->second.begin(),companion->second.end(),assetIndex)!=companion->second.end();
            const unsigned priority=owner?2:1;
            if(texturePriority[assetIndex][name]>=priority)continue;
            const auto existing=std::find(asset.textureNames.begin(),asset.textureNames.end(),name);
            const size_t raster=size_t(existing-asset.textureNames.begin());
            const size_t previous=existing==asset.textureNames.end()?0:rgbaImageDecodedBytes(asset.images[raster]);
            const size_t assetBytes=textureBytes[assetIndex]-previous;
            const size_t populationBytes=totalTextureBytes-previous;
            if(imageBytes>maximumModelTextureBytes||
                assetBytes>maximumModelTextureBytes-imageBytes||
                imageBytes>maximumPopulationTextureBytes-populationBytes)continue;
            textureBytes[assetIndex]=assetBytes+imageBytes;
            totalTextureBytes=populationBytes+imageBytes;
            texturePriority[assetIndex][name]=priority;
            if(existing==asset.textureNames.end()) {asset.images.push_back(image);asset.textureNames.push_back(name);}
            else asset.images[raster]=image;
        }
    };
    std::vector<size_t> textureOrder;
    std::vector<size_t> sharedTextureOrder;
    textureOrder.reserve(entries.size());
    sharedTextureOrder.reserve(entries.size());
    for(size_t index=0;index<entries.size();++index) {
        const auto& entry=entries[index];
        if(entry.detectedExtension!=".chk"&&entry.detectedExtension!=".xtx"&&
            entry.detectedExtension!=".txd")continue;
        if(entry.byteLength>maximumTextureArchiveBytes)continue;
        if(modelStems.count(populationTextureName(entry.name)))
            textureOrder.push_back(index);
        else sharedTextureOrder.push_back(index);
    }
    textureOrder.insert(textureOrder.end(),sharedTextureOrder.begin(),sharedTextureOrder.end());
    const auto missingRequiredRaster=[&]() {
        for(const auto& request:required) {
            for(const size_t owner:request.second) {
                const auto& names=candidates[owner].textureNames;
                if(std::find(names.begin(),names.end(),request.first)==names.end())
                    return true;
            }
        }
        return false;
    };
    for(size_t index:textureOrder) {
        const auto& entry=entries[index];
        const auto fileModels=modelStems.find(populationTextureName(entry.name));
        // Model-owned dictionaries are consumed first. Once every referenced
        // material has been resolved, opening unrelated CHK/XTX archives adds
        // no usable rasters and needlessly decompresses large IMG segments.
        if(fileModels==modelStems.end()&&!missingRequiredRaster())break;
        std::vector<uint8_t> bytes;
        LeedsTextureArchive textures;
        if(!archive.extractDirEntryBytes(index,bytes,error)||
            !textures.loadFromMemory(bytes,LeedsPlatform::Auto,error)) continue;
        for(size_t raster=0;raster<textures.textures().size();++raster) {
            const auto& source=textures.textures()[raster];
            const std::string name=populationTextureName(source.name);
            const auto found=required.find(name);
            if(found==required.end()&&fileModels==modelStems.end()) continue;
            if(source.width<=0||source.height<=0||source.width>2048||source.height>2048||
                uint64_t(source.width)*uint64_t(source.height)*4u>16u*1024u*1024u)continue;
            RgbaImage image;
            if(!textures.decodeTexture(raster,image,error)||image.rgba.empty()) continue;
            assignRaster(entry.name,name,image);
        }
    }
    // Retail PS2 can keep vehicle rasters in GAME.DTZ's native RslTexture
    // dictionaries rather than exposing a companion CHK/XTX in the IMG.
    // Resolve those dictionaries by their material names as well as their
    // model/dictionary name; carcols RGBA values are not raster textures.
    const auto& dtz=archive.unpackedBytes();
    const auto word=[&](size_t offset)->uint32_t {
        if(offset>dtz.size()||dtz.size()-offset<4)return 0;
        return uint32_t(dtz[offset])|(uint32_t(dtz[offset+1])<<8)|
            (uint32_t(dtz[offset+2])<<16)|(uint32_t(dtz[offset+3])<<24);
    };
    const uint32_t textureTable=word(0x60);
    if(textureTable&&uint64_t(textureTable)+12<=dtz.size()) {
        const uint32_t rows=word(textureTable);
        const uint32_t count=word(size_t(textureTable)+8);
        if(rows&&count&&count<=4096&&uint64_t(rows)+uint64_t(count)*28<=dtz.size()) {
            std::set<std::string> wantedNames;
            for(const auto& item:required)wantedNames.insert(item.first);
            unsigned dictionariesDecoded=0;
            for(uint32_t index=0;index<count&&dictionariesDecoded<512;++index) {
                const size_t row=size_t(rows)+size_t(index)*28;
                std::string dictionaryName;
                for(size_t letter=0;letter<20&&dtz[row+8+letter];++letter) {
                    const unsigned char character=dtz[row+8+letter];
                    if(character<32||character>126) {dictionaryName.clear();break;}
                    dictionaryName.push_back(char(character));
                }
                const uint32_t dictionary=word(row);
                if(!dictionary||dictionaryName.empty())continue;
                ++dictionariesDecoded;
                const bool matchingModel=modelStems.count(populationTextureName(dictionaryName))!=0;
                // An unrelated RslTexture dictionary may hold hundreds of
                // rasters. Inspect every name, but decode only material names
                // actually required by a loaded actor or vehicle.
                const auto* filter=matchingModel?nullptr:&wantedNames;
                for(const auto& native:storylandDecodeNativeTextureDictionary(
                    dtz,dictionary,archive.gameVariant()==StorylandDtzGameVariant::Vcs,
                    filter,matchingModel?24u*1024u*1024u:16u*1024u*1024u)) {
                    if(required.count(populationTextureName(native.name))||
                       modelStems.count(populationTextureName(dictionaryName)))
                        assignRaster(dictionaryName,native.name,native.image);
                }
            }
        }
    }
}

// Resolve only authored wheel IDs. Shared ownership follows loaded vehicle assets.
static bool storylandLoadSharedVehicleWheels(const StorylandDtzArchive& archive,
    std::vector<StorylandPlayTestAsset>& vehicles, std::string& error) {
    const bool vcs=archive.gameVariant()==StorylandDtzGameVariant::Vcs;
    if(!vcs&&archive.gameVariant()!=StorylandDtzGameVariant::Lcs)return true;
    std::map<int,std::shared_ptr<StorylandPlayTestAsset>> loaded;
    for(auto& vehicle:vehicles) {
        const int id=vehicle.vehicleDefinition.wheelModelId;
        if(id<=0||!vehicle.vehicleDefinition.hasNativeSuspension)continue;
        auto found=loaded.find(id);
        if(found==loaded.end()) {
            StorylandPlayTestAsset wheel;std::string diagnostic;
            if(!wheel.model.loadNativeSimpleModel(archive.unpackedBytes(),uint32_t(id),diagnostic,vcs)) {
                if (fatalPs2PacketError(diagnostic)) { error = diagnostic; return false; }
                loaded[id]=nullptr;continue;
            }
            wheel.name="wheel_"+std::to_string(id);wheel.materialColors=wheel.model.previewMaterialColors();
            std::vector<StorylandPlayTestAsset> textures;textures.push_back(std::move(wheel));
            storylandLoadPlayTestAssetTextures(archive,textures,diagnostic);
            loaded[id]=std::make_shared<StorylandPlayTestAsset>(std::move(textures.front()));
            found=loaded.find(id);
        }
        vehicle.sharedWheel=found->second;
        if(vehicle.sharedWheel)
            storylandSetupVehicleTyreGeometry(vehicle.vehicleDefinition,vehicle.sharedWheel->model,true);
    }
    return true;
}

static void storylandApplyNativePspDefaultVehiclePaint(const StorylandDtzArchive& archive,
    StorylandPlayTestAsset& asset) {
    if (!archive.isPsp() || archive.gameVariant() != StorylandDtzGameVariant::Vcs ||
        !asset.vehicleDefinition.colors.empty()) return;
    // Native ChooseVehicleColour 0x29DCC8 supplies this RGBA when the set count is zero.
    const auto& slots = asset.vehicleDefinition.materialPaintSlots;
    for (size_t material = 0; material < slots.size() && material < asset.materialColors.size(); ++material)
        if (slots[material] >= 0) asset.materialColors[material] = 0xFF050505u;
}

namespace {
bool setupVcsJetSkiChassis(StorylandVehicleDefinition&,const StorylandModelFile&,std::string&);
}

bool storylandLoadPlayTestPopulation(const StorylandDtzArchive& archive,
    const StorylandModelFile& player,const std::string& playerName,
    std::vector<StorylandPlayTestAsset>& pedestrians,
    std::vector<StorylandPlayTestAsset>& vehicles,std::string& error) {
    pedestrians.clear();vehicles.clear();
    std::vector<StorylandPlayTestAsset> candidates;
    std::set<uint64_t> identities{populationGeometryIdentity(player)};
    const auto& entries=archive.dirEntries();
    // Residency is limited by bytes. Population counts come from native zones
    // and pools rather than requiring a demonstration cast of fixed size.
    constexpr uint64_t maximumModelArchiveBytes=8u*1024u*1024u;
    constexpr size_t maximumGeometryBytes=128u*1024u*1024u;
    size_t geometryBytes=0;
    std::vector<size_t> order(entries.size());
    for(size_t index=0;index<order.size();++index) order[index]=index;
    std::stable_sort(order.begin(),order.end(),[&](size_t left,size_t right) {
        return entries[left].streamingIndex<entries[right].streamingIndex;
    });
    std::vector<std::array<uint32_t,16>> groups;
    std::string groupError;
    const bool authoredGroups=storylandDecodePedestrianGroups(archive.unpackedBytes(),
        archive.gameVariant()==StorylandDtzGameVariant::Vcs,groups,groupError);
    std::set<uint32_t> ambientModels;
    for(const auto& group:groups)ambientModels.insert(group.begin(),group.end());
    const auto ambientCivilian=[&](const StorylandDtzDirEntry& entry) {
        const auto& data=archive.unpackedBytes();
        const auto word=[&](size_t offset) -> uint32_t {
            if(offset>data.size()||data.size()-offset<4) return UINT32_MAX;
            return uint32_t(data[offset])|(uint32_t(data[offset+1])<<8)|(uint32_t(data[offset+2])<<16)|(uint32_t(data[offset+3])<<24);
        };
        const uint32_t table=word(0x3C);
        if(table==UINT32_MAX||entry.streamingIndex==UINT32_MAX||uint64_t(table)+uint64_t(entry.streamingIndex)*4+4>data.size()) return false;
        const uint32_t info=word(size_t(table)+size_t(entry.streamingIndex)*4);
        if(info==UINT32_MAX) return false;
        const uint32_t type=word(size_t(info)+(archive.gameVariant()==StorylandDtzGameVariant::Vcs?0x34:0x30));
        if(type!=4&&type!=5) return false;
        if(authoredGroups)return ambientModels.count(entry.streamingIndex)!=0;
        const auto name=populationTextureName(entry.name);
        static const std::set<std::string> special={"army","soldier","military","cop","swat","fbi","player","plr",
            "vic","victor","vance","lance","louise","phil","jerry","marty","diaz","ricardo","reni","bryan",
            "forbes","gonzales","armando","diego","mendez","biker","tony","toni","salvatore","maria",
            "donald","leone","jd","wayne","jane","mickey","miguel","kazuki","toshiko","leon","paulie"};
        return !special.count(name)&&name.compare(0,3,"cs_")!=0&&name.compare(0,4,"cut_")!=0;
    };
    for(size_t index:order) {
        const auto& entry=entries[index];
        if(entry.detectedExtension!=".mdl" ||
            entry.byteLength>maximumModelArchiveBytes||
            populationTextureName(entry.name)==populationTextureName(playerName)) continue;
        const auto& native = archive.unpackedBytes();
        const auto nativeWord = [&](size_t offset) -> uint32_t {
            if (offset > native.size() || native.size() - offset < 4) return 0;
            return uint32_t(native[offset]) | (uint32_t(native[offset + 1]) << 8) |
                (uint32_t(native[offset + 2]) << 16) | (uint32_t(native[offset + 3]) << 24);
        };
        const uint32_t table = nativeWord(0x3C);
        if (table && entry.streamingIndex < nativeWord(0x38) &&
            uint64_t(table) + uint64_t(entry.streamingIndex) * 4 + 4 <= native.size()) {
            const uint32_t info = nativeWord(size_t(table) + size_t(entry.streamingIndex) * 4);
            if (info && uint64_t(info) + 0x11 <= native.size()) {
                const uint8_t kind = native[info + 0x10];
                if (kind != 6 && kind != 7) continue;
                if (kind == 7 && !ambientCivilian(entry)) continue;
            }
        }
        std::vector<uint8_t> bytes;
        StorylandPlayTestAsset asset;
        try {
            if(!archive.extractDirEntryBytes(index,bytes,error) ||
                !asset.model.loadFromMemory(bytes,std::wstring(entry.name.begin(),entry.name.end()),error)) {
                if (fatalPs2PacketError(error)) return false;
                continue;
            }
        } catch(const std::bad_alloc&) {
            error="Out of memory while loading a retail pedestrian/vehicle MDL.";
            return false;
        } catch(const std::exception&) {
            // An unsupported retail model must not terminate the Win32 UI.
            continue;
        }
        const bool ped=asset.model.modelKind()==StorylandModelKind::PedModel;
        const bool vehicle=asset.model.modelKind()==StorylandModelKind::VehicleModel;
        if(ped)asset.pedStats=storylandDecodePedStats(archive.unpackedBytes(),entry.streamingIndex,
            archive.gameVariant()==StorylandDtzGameVariant::Vcs);
        if((!ped&&!vehicle)||(ped&&!ambientCivilian(entry))||
            asset.model.previewPoints().empty()||asset.model.previewTriangles().empty()||
            (ped&&asset.model.armatureBones().empty())) continue;
        if(vehicle) {
            asset.vehicleDefinition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),entry.streamingIndex,
                archive.gameVariant()==StorylandDtzGameVariant::Vcs);
            asset.vehicleDefinition.previewRoot=asset.model.vehiclePreviewRootMatrix();
            asset.vehicleDefinition.materialPaintSlots=asset.model.vehicleMaterialPaintSlots(
                archive.gameVariant()==StorylandDtzGameVariant::Vcs);
            if(!asset.model.previewPoints().empty()) {
                asset.vehicleDefinition.previewMinimumZ=asset.vehicleDefinition.previewMaximumZ=asset.model.previewPoints().front().z;
                for(const auto& point:asset.model.previewPoints()) {
                    asset.vehicleDefinition.previewMinimumZ=std::min(asset.vehicleDefinition.previewMinimumZ,point.z);
                    asset.vehicleDefinition.previewMaximumZ=std::max(asset.vehicleDefinition.previewMaximumZ,point.z);
                }
            }
            storylandSetupVehicleSuspension(asset.vehicleDefinition,asset.model);
            if(!setupVcsJetSkiChassis(asset.vehicleDefinition,asset.model,error))return false;
            asset.vehicleDomain=storylandVehicleDomain(archive.unpackedBytes(),entry.streamingIndex,
                archive.gameVariant()==StorylandDtzGameVariant::Vcs);
            if(asset.vehicleDomain!=StorylandVehicleDomain::Road &&
               asset.vehicleDomain!=StorylandVehicleDomain::Water) continue;
        }
        const size_t bytesNeeded=asset.model.rawBytes().size()+
            asset.model.previewPoints().size()*sizeof(StorylandModelPoint)+
            asset.model.previewTriangles().size()*sizeof(StorylandModelTriangle)+
            asset.model.previewSkinWeights().size()*sizeof(StorylandModelSkinWeights);
        if(bytesNeeded>maximumGeometryBytes-geometryBytes)continue;
        asset.materialColors=asset.model.previewMaterialColors();
        if (vehicle) storylandApplyNativePspDefaultVehiclePaint(archive, asset);
        asset.geometryIdentity=populationGeometryIdentity(asset.model);
        const bool nativeIdentity=(ped&&asset.pedStats.valid)||
            (vehicle&&asset.vehicleDefinition.modelId!=UINT32_MAX);
        if(!nativeIdentity&&!identities.insert(asset.geometryIdentity).second)continue;
        asset.name=entry.name;
        geometryBytes+=bytesNeeded;
        candidates.push_back(std::move(asset));
    }
    storylandLoadPlayTestAssetTextures(archive,candidates,error);
    for(auto& asset:candidates) {
        if(asset.images.empty()) continue;
        if(asset.model.modelKind()==StorylandModelKind::PedModel)
            pedestrians.push_back(std::move(asset));
        else if(asset.model.modelKind()==StorylandModelKind::VehicleModel)
            vehicles.push_back(std::move(asset));
    }
    if(pedestrians.empty()&&vehicles.empty()) {
        error="No textured native ambient pedestrian or vehicle models were found.";
        return false;
    }
    if (!storylandLoadSharedVehicleWheels(archive,vehicles,error)) return false;
    error.clear();return true;
}

namespace {
bool setupVcsJetSkiChassis(StorylandVehicleDefinition& definition,
    const StorylandModelFile& model,std::string& error) {
    if(!definition.vcsJetSki())return true;
    StorylandVcsJetSkiChassis chassis;std::string name;
    if(!model.vehicleComponentFrame(1,chassis.local,name,&chassis.parent,&chassis.pointMask)||
        !storylandVcsJetSkiInverse(storylandVcsJetSkiMatrixProduct(chassis.parent,chassis.local),
            chassis.inverseBind)) {
        error="Original VCS JetSki component1 hierarchy or geometry is unavailable.";return false;
    }
    chassis.available=true;definition.vcsJetSkiChassis=std::move(chassis);return true;
}
}

bool storylandLoadPlayTestVehicle(const StorylandDtzArchive& archive,uint32_t modelId,
    StorylandPlayTestAsset& asset,std::string& error) {
    size_t index=0;std::string name;
    if(!archive.findModelDirEntryByModelId(modelId,index,name)) {error="Requested retail vehicle model is absent.";return false;}
    const auto& entry=archive.dirEntries()[index];
    if(entry.byteLength>8u*1024u*1024u) {error="Requested vehicle exceeds the model allocation budget.";return false;}
    std::vector<uint8_t> bytes;StorylandPlayTestAsset candidate;
    if(!archive.extractDirEntryBytes(index,bytes,error))return false;
    const bool vcs=archive.gameVariant()==StorylandDtzGameVariant::Vcs;
    candidate.vehicleDefinition=storylandDecodeVehicleDefinition(archive.unpackedBytes(),modelId,vcs);
    // Native boat model-info selects the full vehicle clump, independent of
    // frame-name hints or whichever atomic happens to occur first in the file.
    const bool nativeBoat=candidate.vehicleDefinition.domain==StorylandVehicleDomain::Water&&
        (candidate.vehicleDefinition.nativeType==1||(vcs&&candidate.vehicleDefinition.nativeType==2))&&
        bytes.size()>=4&&bytes[0]=='l'&&bytes[1]=='d'&&bytes[2]=='m'&&bytes[3]==0;
    const auto displayName=std::wstring(name.begin(),name.end());
    if(!(nativeBoat?candidate.model.loadNativeVehicleFromMemory(bytes,displayName,error):
        candidate.model.loadFromMemory(bytes,displayName,error)))return false;
    if(candidate.model.modelKind()!=StorylandModelKind::VehicleModel||
        candidate.model.previewPoints().empty()||candidate.model.previewTriangles().empty()) {
        error="Requested model is not a decoded vehicle.";return false;
    }
    candidate.name=name;
    candidate.vehicleDefinition.previewRoot=candidate.model.vehiclePreviewRootMatrix();
    candidate.vehicleDefinition.materialPaintSlots=candidate.model.vehicleMaterialPaintSlots(
        archive.gameVariant()==StorylandDtzGameVariant::Vcs);
    candidate.vehicleDomain=candidate.vehicleDefinition.domain;
    storylandSetupVehicleSuspension(candidate.vehicleDefinition,candidate.model);
    if(!setupVcsJetSkiChassis(candidate.vehicleDefinition,candidate.model,error))return false;
    candidate.materialColors=candidate.model.previewMaterialColors();
    storylandApplyNativePspDefaultVehiclePaint(archive, candidate);
    candidate.geometryIdentity=populationGeometryIdentity(candidate.model);
    std::vector<StorylandPlayTestAsset> candidates;candidates.push_back(std::move(candidate));
    storylandLoadPlayTestAssetTextures(archive,candidates,error);
    if(candidates.front().images.empty()) {error="Requested vehicle has no resolved retail textures.";return false;}
    if (!storylandLoadSharedVehicleWheels(archive,candidates,error)) return false;
    asset=std::move(candidates.front());error.clear();return true;
}

namespace {
std::wstring retailLower(std::wstring value) {
    std::transform(value.begin(),value.end(),value.begin(),[](wchar_t character) {
        return wchar_t(std::towlower(character));
    });
    return value;
}
bool retailAssetFolder(const std::filesystem::path& path) {
    static const std::set<std::wstring> names={
        L"audio",L"chk",L"configs",L"frontend",L"icons",L"mall",L"mc0",L"mc1",L"modules",
        L"movies",L"ps2",L"psp",L"splash",L"scripts",L"data",L"models",
        L"usrdir",L"psp_game",L"rundata",L"beach",L"mainla",L"game",L"music",L"sfx",
        L"radio",L"speech",L"streams",L"effects",L"anim",L"animations",
        L"text",L"textures",L"cutscene",L"cutscenes",L"lvz",L"img",
        L"assets",L"files",L"maps",L"map",L"levels",L"level",
        L"interiors",L"areas",L"ipl",L"placements"
    };
    return names.count(retailLower(path.filename().wstring()))!=0;
}
std::filesystem::path retailGameRoot(const std::wstring& dtzPath,
    const std::wstring& worldPath,const std::wstring& scmPath=L"") {
    const auto& source=!dtzPath.empty()?dtzPath:!worldPath.empty()?worldPath:scmPath;
    if(source.empty())return {};
    std::error_code error;
    auto root=std::filesystem::absolute(std::filesystem::path(source),error).parent_path();
    if(error)return {};
    root=root.lexically_normal();
    for(unsigned level=0;level<6&&retailAssetFolder(root)&&retailLower(root.filename().wstring())!=L"game";++level) {
        const auto parent=root.parent_path();
        if(parent.empty()||parent==root||parent==parent.root_path())break;
        root=parent;
    }
    return root;
}
std::filesystem::path retailSubdirectory(std::filesystem::path root,
    const std::filesystem::path& suffix) {
    for(const auto& component:suffix) {
        if(component.empty()||component==L".")continue;
        std::error_code error;
        std::filesystem::directory_iterator cursor(root,
            std::filesystem::directory_options::skip_permission_denied,error),end;
        bool found=false;
        for(;!error&&cursor!=end;cursor.increment(error)) {
            if(retailLower(cursor->path().filename().wstring())!=retailLower(component.wstring())||
                !cursor->is_directory(error)||cursor->is_symlink(error))continue;
            root=cursor->path();found=true;break;
        }
        if(!found)return {};
    }
    return root;
}
}

bool storylandFindRequiredPlayTestScm(const std::wstring& dtzPath,const std::wstring& worldPath,
    bool vcs,std::wstring& result,std::string& error,const StorylandVcsModloader* mods) {
    result.clear();
    if (vcs && mods && storylandFindNativeGameFile(mods->gameRoot(), "MAIN.SCM").empty()) {
        const auto effective = mods->resolveGameFile("MAIN.SCM");
        if (!effective.empty()) {
            StorylandScmFile script;
            if (!script.loadFromFile(effective.wstring(), error, false) || !script.header().valid) {
                error = "Modloader MAIN.SCM rejected: " + effective.u8string() + ": " +
                    (error.empty() ? "unrecognized retail SCM header" : error);
                return false;
            }
            result = effective.wstring();
            error.clear();
            return true;
        }
    }
    const std::vector<std::filesystem::path> suffixes = vcs ?
        std::vector<std::filesystem::path>{L"SCRIPTS",L"DATA",L"PSP_GAME/USRDIR/SCRIPTS",
            L"PSP_GAME/USRDIR/DATA",L"PSP_GAME/USRDIR",L""} :
        std::vector<std::filesystem::path>{L"DATA",L"PSP_GAME/USRDIR/DATA",
            L"SCRIPTS",L"PSP_GAME/USRDIR/SCRIPTS",L"PSP_GAME/USRDIR",L""};
    std::vector<std::filesystem::path> roots;
    std::set<std::filesystem::path> visited;
    for(const auto& source:{dtzPath,worldPath}) {
        if(source.empty())continue;
        for(const auto& folder:storylandNativeGameFolders(std::filesystem::path(source)))
            if(visited.insert(folder).second)roots.push_back(folder);
    }
    const auto gameRoot=retailGameRoot(dtzPath,worldPath);
    if(!gameRoot.empty())for(const auto& suffix:suffixes) {
        const auto candidate=retailSubdirectory(gameRoot,suffix);
        if(!candidate.empty()&&visited.insert(candidate).second)roots.push_back(candidate);
    }
    std::string firstRejected;
    for(const auto& root:roots) {
        std::error_code ec;
        std::filesystem::directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
        for(;!ec&&it!=end;it.increment(ec)) {
            const auto entry=*it;
            if(clipLower(entry.path().filename().string())!="main.scm"||!entry.is_regular_file(ec))continue;
            const auto effective = vcs && mods ? mods->resolveRetailPath(entry.path()) : entry.path();
            StorylandScmFile script;
            std::string scriptError;
            if(!script.loadFromFile(effective.wstring(),scriptError,false)) {
                if(effective != entry.path()) { error = "Modloader MAIN.SCM rejected: " + effective.u8string() + ": " + scriptError; return false; }
                if(firstRejected.empty())firstRejected=effective.u8string()+": "+scriptError;
                continue;
            }
            if(!script.header().valid) {
                if(effective != entry.path()) { error = "Modloader MAIN.SCM rejected: " + effective.u8string() + ": unrecognized retail SCM header"; return false; }
                if(firstRejected.empty())firstRejected=effective.u8string()+": unrecognized retail SCM header";
                continue;
            }
            // The byte at +0x0F identifies an SCM header dialect, not the
            // title. Both verified retail PS2 LCS and VCS MAIN.SCM files use
            // 'm'. Select the game from GAME.DTZ and the directory layout.
            result=effective.wstring();error.clear();return true;
        }
    }
    error=std::string(vcs?"VCS":"LCS")+
        " Play Test requires its own retail MAIN.SCM (LCS: DATA; VCS: RUNDATA or SCRIPTS).";
    if(!firstRejected.empty())error+=" Rejected candidate: "+firstRejected;
    return false;
}

StorylandRetailAssetIndex storylandDiscoverRetailAssets(
    const std::wstring& dtzPath, const std::wstring& worldPath,
    const std::wstring& scmPath, const StorylandVcsModloader* mods) {
    StorylandRetailAssetIndex index;
    std::set<std::filesystem::path> directories;
    std::set<std::filesystem::path> inspected;
    constexpr size_t maximumFiles = 12000;
    for(const auto& source:{dtzPath,worldPath,scmPath}) {
        if(source.empty())continue;
        const auto root=retailGameRoot(source,L"");
        if(!root.empty())directories.insert(root);
    }
    if(directories.empty())return index;
    const auto lowercase=[](std::wstring value) {return retailLower(std::move(value));};
    const auto appendAsset = [&](const std::filesystem::path& logical,
        const std::filesystem::path& effective) {
        if(effective.empty())return false;
        const auto extension = lowercase(logical.extension().wstring());
        std::string group;
        if (extension == L".pmf" || extension == L".pss" || extension == L".mpg" || extension == L".mpeg") group = "Movie";
        else if (extension == L".vb" || extension == L".sdt" || extension == L".wav" ||
                 extension == L".at3" || extension == L".adx" || extension == L".vag" ||
                 extension == L".mp3" || extension == L".ogg" || extension == L".aa3" ||
                 extension == L".oma" || extension == L".raw" || extension == L".aac" ||
                 extension == L".m4a" || extension == L".wma" || extension == L".flac") group = "Audio";
        else if (extension == L".scm" || extension == L".gxt") group = "Script";
        else if (extension == L".dir" || extension == L".anim" || extension == L".cut" ||
                 extension == L".cam" || extension == L".ifp") group = "Animation";
        else if (extension == L".mdl" || extension == L".chk" || extension == L".xtx" ||
                 extension == L".img" || extension == L".lvz" || extension == L".dtz" ||
                 extension == L".dff" || extension == L".txd" || extension == L".ipl" || extension == L".enex") group = "World";
        if(group.empty())return false;
        if(!inspected.insert(effective.lexically_normal()).second)return false;
        std::error_code error;
        const auto bytes = std::filesystem::file_size(effective,error);
        if(error)return false;
        index.files.push_back({effective.wstring(),group,uint64_t(bytes)});
        if(group == "Movie")++index.movieCount;
        else if(group == "Audio")++index.audioCount;
        else if(group == "Script")++index.scriptCount;
        else ++index.modelCount;
        if(index.files.size() < maximumFiles)return false;
        index.truncated = true;
        return true;
    };
    for (const auto& directory : directories) {
        std::error_code ec;
        std::filesystem::recursive_directory_iterator cursor(directory,
            std::filesystem::directory_options::skip_permission_denied, ec), end;
        for (; !ec && cursor != end; cursor.increment(ec)) {
            if (cursor.depth() > 5) { cursor.disable_recursion_pending(); continue; }
            if (cursor.depth()==0&&cursor->is_directory(ec)&&!retailAssetFolder(cursor->path())) {
                cursor.disable_recursion_pending();continue;
            }
            if (cursor->is_symlink(ec)) { cursor.disable_recursion_pending(); continue; }
            if (!cursor->is_regular_file(ec)) continue;
            const auto original = cursor->path().lexically_normal();
            const auto effective = mods ? mods->resolveRetailPath(original) : original;
            if(appendAsset(original,effective))return index;
        }
    }
    if(mods)for(const auto& file:mods->files()) {
        if(appendAsset(std::filesystem::path(file.first),file.second))return index;
    }
    std::sort(index.files.begin(), index.files.end(), [](const auto& first, const auto& second) {
        return first.path < second.path;
    });
    return index;
}

bool storylandLoadPlayTestWeapons(const StorylandDtzArchive& archive,
    std::vector<StorylandWeaponDefinition>& definitions,std::vector<StorylandPlayTestAsset>& assets,
    std::string& diagnostic) {
    definitions={StorylandWeaponDefinition{}};assets.clear();
    const auto& bytes=archive.unpackedBytes();
    const auto word=[&](size_t offset) -> uint32_t {
        if(offset>bytes.size()||bytes.size()-offset<4) return 0;
        return uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|
            (uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);
    };
    const auto real=[&](size_t offset) {
        const uint32_t bits=word(offset);float result=0;std::memcpy(&result,&bits,4);return result;
    };
    size_t table=0,stride=0,count=0;
    bool vcsLayout=false,ps2LcsLayout=false;
    for(const auto& block:archive.dataBlocks()) {
        if(block.parser=="vcs-weapon-0x70-records"&&block.rowSize==0x70&&block.rowCount<=64) {
            table=block.offset;stride=block.rowSize;count=block.rowCount;vcsLayout=true;break;
        }
    }
    if(!table&&archive.gameVariant()==StorylandDtzGameVariant::Lcs) {
        // The PS2 ResourceImage points to a weapon-table owner, whose first
        // pointer selects the active records. Its aligned CVector makes each
        // row 0x70 bytes; the compact reLCS/PC 0x64 layout is different.
        const size_t owner=word(0xBC),candidate=word(owner);
        bool valid=owner>=0x20&&owner<=bytes.size()&&bytes.size()-owner>=8&&
            candidate>=0x20&&candidate<=bytes.size()&&bytes.size()-candidate>=37*0x70;
        for(size_t row=0;valid&&row<37;++row) {
            const size_t base=candidate+row*0x70;
            const int32_t model=int32_t(word(base+0x60));
            size_t entry=0;std::string name;
            valid=word(base)<=4&&std::isfinite(real(base+4))&&real(base+4)>0&&real(base+4)<=1000&&
                word(base+0x68)<10&&(word(base+0x6C)&0xFFC00000u)==0&&
                ((row==0&&model==-1)||(model>=0&&archive.findModelDirEntryByModelId(uint32_t(model),entry,name)));
        }
        if(valid) {table=candidate;stride=0x70;count=37;ps2LcsLayout=true;}
    }
    if(!table&&archive.gameVariant()==StorylandDtzGameVariant::Lcs) {
        // reLCS CWeaponInfo has a 0x64-byte layout. Validate model IDs against
        // the loaded streaming map rather than borrowing a retail table address.
        const size_t candidate=word(0xBC);
        size_t valid=0;
        if(candidate&&candidate<=bytes.size()&&bytes.size()-candidate>=37*0x64) {
            for(size_t row=0;row<37;++row) {
                const size_t base=candidate+row*0x64;
                size_t entry=0;std::string name;
                if(word(base)<=4&&std::isfinite(real(base+4))&&real(base+4)>0&&real(base+4)<=1000&&
                    archive.findModelDirEntryByModelId(word(base+0x54),entry,name)) ++valid;
            }
            if(valid>=5) {table=candidate;stride=0x64;count=37;}
        }
    }
    if(!table) {diagnostic="No validated native weapon table; fists are available.";return false;}
    for(size_t row=0;row<count;++row) {
        const size_t base=table+row*stride;
        if(base>bytes.size()||bytes.size()-base<stride) break;
        const size_t shift=vcsLayout?4:0;
        const int fireType=int32_t(word(base+shift));
        const bool nativeCamera=fireType==4&&(vcsLayout?(row==38u||row==39u):row==36u);
        if(fireType<0||(fireType>3&&!nativeCamera)) continue;
        const float range=real(base+shift+4);
        if(!std::isfinite(range)||range<=0)continue;
        // CWeaponInfo's aligned CVector in the PS2 resource image; the compact
        // reLCS layout places the same three components twelve bytes earlier.
        const size_t fireOffsetField=vcsLayout||ps2LcsLayout?0x30:0x24;
        const std::array<float,3> fireOffset{real(base+fireOffsetField),
            real(base+fireOffsetField+4),real(base+fireOffsetField+8)};
        const bool fireOffsetValid=std::all_of(fireOffset.begin(),fireOffset.end(),
            [](float value){return std::isfinite(value);});
        if(row==0) {
            definitions[0].nativeId=0;
            definitions[0].targetRange=range;
            definitions[0].fireOffset=fireOffset;definitions[0].fireOffsetValid=fireOffsetValid;
            definitions[0].nativeFlags=word(base+(vcsLayout?0:ps2LcsLayout?0x6C:0x5C));
            definitions[0].animationGroup=int32_t(word(base+(vcsLayout||ps2LcsLayout?0x40:0x34)));
            definitions[0].damage=float(std::clamp(int32_t(word(base+shift+0x14)),1,1000));
            definitions[0].interval=std::clamp(float(word(base+shift+8))/1000.0f,0.05f,5.0f);
        }
        const int modelId=int32_t(word(base+(vcsLayout||ps2LcsLayout?0x60:0x54)));
        size_t entry=0;std::string name;
        if(row==0) continue;
        StorylandPlayTestAsset asset;
        bool hasVisual=false;
        if(modelId>=0&&archive.findModelDirEntryByModelId(uint32_t(modelId),entry,name)) {
            std::vector<uint8_t> modelBytes;std::string error;
            hasVisual=archive.extractDirEntryBytes(entry,modelBytes,error)&&
                asset.model.loadFromMemory(modelBytes,std::wstring(name.begin(),name.end()),error)&&
                !asset.model.previewPoints().empty()&&!asset.model.previewTriangles().empty();
            if (fatalPs2PacketError(error)) { diagnostic = error; return false; }
            if(hasVisual) {
                asset.name=name;
                asset.materialColors=asset.model.previewMaterialColors();
            }
        }
        StorylandWeaponDefinition definition;
        {
            definition.nativeFlags=word(base+(vcsLayout?0:ps2LcsLayout?0x6C:0x5C));
            const size_t animation=vcsLayout||ps2LcsLayout?0x40:0x34;
            definition.animationGroup=int32_t(word(base+animation));
            const float start=real(base+animation+4),end=real(base+animation+8),fire=real(base+animation+12);
            if(vcsLayout) {
                definition.crouchLoopStart=real(base+0x50);
                definition.crouchLoopEnd=real(base+0x54);
                definition.crouchFireTime=real(base+0x58);
            }
            if(std::isfinite(start)&&std::isfinite(end)&&std::isfinite(fire)&&
                start>=0&&end>start&&end<30&&fire>=start&&fire<=end) {
                definition.animationLoopStart=start;definition.animationLoopEnd=end;definition.animationFireTime=fire;
            }
        }
        definition.nativeId=int(row);definition.modelId=modelId;definition.fireType=fireType;
        definition.fireOffset=fireOffset;definition.fireOffsetValid=fireOffsetValid;
        definition.name=name.empty()?"Weapon "+std::to_string(row):std::filesystem::path(name).stem().string();
        definition.slot=int32_t(word(base+(vcsLayout||ps2LcsLayout?0x68:0x60)));
        definition.targetRange=range;
        definition.range=fireType?range:std::clamp(range,0.5f,3.0f);
        definition.damage=float(std::clamp(int32_t(word(base+shift+0x14)),nativeCamera?0:1,1000));
        definition.interval=std::clamp(float(word(base+shift+8))/1000.0f,0.05f,5.0f);
        definition.reloadSeconds=std::clamp(float(word(base+shift+0x0C))/1000.0f,0.2f,10.0f);
        definition.clipSize=nativeCamera?0:std::clamp(int32_t(word(base+shift+0x10)),1,500);
        const float speed=real(base+shift+0x18),radius=real(base+shift+0x1C);
        if(std::isfinite(speed)&&speed>0) definition.speed=speed;
        if(std::isfinite(radius)&&radius>0) definition.radius=radius;
        definition.gravity=(definition.nativeFlags&1)!=0;
        const float lifetime=real(base+shift+0x20);
        if(std::isfinite(lifetime)&&lifetime>0) definition.lifetime=std::clamp(lifetime,0.2f,30.0f);
        if(hasVisual) {
            definition.visual=assets.size();
            assets.push_back(std::move(asset));
        }
        definitions.push_back(std::move(definition));
    }
    size_t missileEntry=0;
    bool missileFound=archive.findDirEntryByStemAndExtension(L"missile",{L".mdl",L".dff"},missileEntry);
    if(!missileFound)missileFound=archive.findDirEntryByStemAndExtension(L"rocket",{L".mdl",L".dff"},missileEntry);
    if(missileFound && missileEntry<archive.dirEntries().size()) {
        StorylandPlayTestAsset missile;std::vector<uint8_t> payload;std::string error;
        missile.name=archive.dirEntries()[missileEntry].name;
        if(archive.extractDirEntryBytes(missileEntry,payload,error) &&
            missile.model.loadFromMemory(payload,std::wstring(missile.name.begin(),missile.name.end()),error) &&
            !missile.model.previewPoints().empty() && !missile.model.previewTriangles().empty()) {
            missile.materialColors=missile.model.previewMaterialColors();
            for(auto& definition:definitions) {
                const auto name=clipLower(definition.name);
                if((vcsLayout&&definition.nativeId==18)||name.find("rocket")!=std::string::npos||
                    name=="rlaunch"||name=="rpg")definition.projectileVisual=assets.size();
            }
            assets.push_back(std::move(missile));
        }
        if (fatalPs2PacketError(error)) { diagnostic = error; return false; }
    }
    if(vcsLayout) {
        // Native 180BA4 creates weapon15's projectile using the hardcoded
        // model-index table +0x82, independently of its pickup model.
        const size_t modelIndices=word(0x5C);
        if(modelIndices<=bytes.size()&&bytes.size()-modelIndices>=0x84) {
            const uint32_t modelId=uint32_t(bytes[modelIndices+0x82])|
                (uint32_t(bytes[modelIndices+0x83])<<8);
            size_t entry=0;std::string name;
            if(archive.findModelDirEntryByModelId(modelId,entry,name)) {
                std::vector<StorylandPlayTestAsset> mineAssets;std::string error;
                if(storylandLoadPlayTestModels(archive,{name},mineAssets,error)&&mineAssets.size()==1) {
                    for(auto& definition:definitions)
                        if(definition.nativeId==15)definition.projectileVisual=assets.size();
                    assets.push_back(std::move(mineAssets.front()));
                }
                if(fatalPs2PacketError(error)) {diagnostic=error;return false;}
            }
        }
    }
    // Model texture dictionaries may be per-weapon or shared by the weapon set.
    const auto& entries=archive.dirEntries();
    for(size_t index=0;index<entries.size();++index) {
        const auto& entry=entries[index];
        if(entry.detectedExtension!=".chk"&&entry.detectedExtension!=".xtx") continue;
        const auto stem=populationTextureName(entry.name);
        const bool shared=stem=="weapons"||stem=="weapon"||stem=="generic";
        if(!shared&&std::none_of(assets.begin(),assets.end(),[&](const auto& asset){return populationTextureName(asset.name)==stem;})) continue;
        std::vector<uint8_t> textureBytes;std::string error;LeedsTextureArchive textures;
        if(!archive.extractDirEntryBytes(index,textureBytes,error)||!textures.loadFromMemory(textureBytes,LeedsPlatform::Auto,error)) continue;
        for(size_t raster=0;raster<textures.textures().size();++raster) {
            const auto textureName=populationTextureName(textures.textures()[raster].name);
            RgbaImage image;
            if(!textures.decodeTexture(raster,image,error)) continue;
            for(auto& asset:assets) {
                if(std::find(asset.textureNames.begin(),asset.textureNames.end(),textureName)!=asset.textureNames.end()) continue;
                const auto& names=asset.model.previewMaterialTextureNames();
                const auto& environmentNames=asset.model.previewMaterialEnvironmentNames();
                const bool materialRequired=std::any_of(names.begin(),names.end(),
                    [&](const auto& name){return populationTextureName(name)==textureName;});
                const bool environmentRequired=std::any_of(environmentNames.begin(),environmentNames.end(),
                    [&](const auto& name){return populationTextureName(name)==textureName;});
                if(populationTextureName(asset.name)!=stem&&!materialRequired&&!environmentRequired) continue;
                asset.images.push_back(image);asset.textureNames.push_back(textureName);
            }
        }
    }
    diagnostic=std::to_string(definitions.size()-1)+" native weapon definitions and model assets loaded.";
    return definitions.size()>1;
}


bool storylandLoadPlayTestWaterTexture(const StorylandDtzArchive& archive,RgbaImage& image,std::string& error) {
    image={};
    auto textures=storylandFindNativeDictionaryTextures(archive.unpackedBytes(),
        archive.gameVariant()==StorylandDtzGameVariant::Vcs,"particle",{"waterclear256"});
    if(!textures.empty()) {image=std::move(textures.front().image);error.clear();return true;}
    error="Native waterclear256 was not found; using the packaged water texture.";return false;
}


bool storylandLoadPlayTestModels(const StorylandDtzArchive& archive,const std::vector<std::string>& names,
    std::vector<StorylandPlayTestAsset>& assets,std::string& error) {
    assets.clear();
    if(names.empty()||names.size()>128) {error="Mission model request exceeds the native actor capacity.";return false;}
    std::vector<StorylandPlayTestAsset> candidates;
    const auto stem=[](std::string name) {
        name=clipLower(name);
        const auto separator=name.find_last_of("/\\");
        if(separator!=std::string::npos)name.erase(0,separator+1);
        const auto extension=name.find_last_of('.');
        if(extension!=std::string::npos)name.resize(extension);
        return name;
    };
    for(const auto& requested:names) {
        const auto wanted=stem(requested);
        bool loaded=false;
        for(size_t index=0;index<archive.dirEntries().size();++index) {
            const auto& entry=archive.dirEntries()[index];
            if((entry.detectedExtension!=".mdl"&&entry.detectedExtension!=".dff")||stem(entry.name)!=wanted)continue;
            if(entry.byteLength>8u*1024u*1024u) {error="Mission model exceeds the allocation limit: "+requested;return false;}
            std::vector<uint8_t> bytes;StorylandPlayTestAsset asset;
            if(!archive.extractDirEntryBytes(index,bytes,error)||
                !asset.model.loadFromMemory(bytes,std::wstring(entry.name.begin(),entry.name.end()),error))return false;
            if(asset.model.previewPoints().empty()||asset.model.previewTriangles().empty()) {
                error="Mission model has no decoded geometry: "+requested;return false;
            }
            asset.name=entry.name;asset.materialColors=asset.model.previewMaterialColors();
            if(asset.model.modelKind()==StorylandModelKind::PedModel)
                asset.pedStats=storylandDecodePedStats(archive.unpackedBytes(),entry.streamingIndex,
                    archive.gameVariant()==StorylandDtzGameVariant::Vcs);
            asset.geometryIdentity=populationGeometryIdentity(asset.model);
            candidates.push_back(std::move(asset));loaded=true;break;
        }
        if(!loaded) {error="Mission model was not found in the retail archive: "+requested;return false;}
    }
    storylandLoadPlayTestAssetTextures(archive,candidates,error);
    assets=std::move(candidates);error.clear();return true;
}

bool storylandSetZombiePedestrians(StorylandPlayTestRuntime& runtime,
    const StorylandDtzArchive& archive,std::vector<StorylandPlayTestAsset>& assets,
    StorylandPlayTestAnimation& animation,bool enabled,std::string& error) {
    if(!enabled) {
        runtime.zombiePedestrians=false;
        for(auto& pedestrian:runtime.pedestrians)storylandRestoreZombiePedestrian(pedestrian);
        error.clear();return true;
    }
    if(archive.gameVariant()!=StorylandDtzGameVariant::Vcs) {
        error="Zombie Pedestrians requires Vice City Stories and its Brawn of the Dead assets.";
        return false;
    }
    size_t unarmed=SIZE_MAX;
    for(size_t index=0;index<runtime.weapons.size();++index)
        if(runtime.weapons[index].nativeId==0) {unarmed=index;break;}
    if(unarmed==SIZE_MAX||runtime.pedestrianModelStats.size()!=assets.size()||
        runtime.pedestrianModelRoles.size()!=assets.size()) {
        error="Start Play Game or Play Test before enabling Zombie Pedestrians.";
        return false;
    }
    size_t model=SIZE_MAX;
    for(size_t index=0;index<assets.size();++index)
        if(assets[index].pedStats.valid&&assets[index].pedStats.modelId==StorylandBrawnZombie::model) {
            model=index;break;
        }
    std::vector<StorylandPlayTestAsset> decoded;
    if(model==SIZE_MAX) {
        size_t entry=0;std::string name;
        if(!archive.findModelDirEntryByModelId(StorylandBrawnZombie::model,entry,name)) {
            error="The original Brawn of the Dead zombie model is unavailable.";
            return false;
        }
        if(!storylandLoadPlayTestModels(archive,{name},decoded,error))return false;
        if(decoded.size()!=1||!decoded.front().pedStats.valid||
            decoded.front().pedStats.modelId!=StorylandBrawnZombie::model||decoded.front().images.empty()) {
            error="The original Brawn of the Dead zombie model or textures are incomplete.";
            return false;
        }
    }
    if(!animation.loadAnimationGroup(archive,StorylandBrawnZombie::animationGroup,error)||
        !animation.selectLocomotionGroup(StorylandBrawnZombie::animationGroup,error))return false;
    const float walkSpeed=animation.locomotionSpeed(StorylandPlayTestMotion::Walk);
    if(!std::isfinite(walkSpeed)||walkSpeed<=0) {
        error="The original Brawn of the Dead zombie walk animation has no valid movement.";
        return false;
    }
    if(model==SIZE_MAX) {
        model=assets.size();
        runtime.pedestrianModelStats.push_back(decoded.front().pedStats);
        runtime.pedestrianModelRoles.push_back(StorylandServiceRole::Civilian);
        assets.push_back(std::move(decoded.front()));
    }
    runtime.zombiePedestrianModel=model;
    runtime.zombiePedestrianWeapon=unarmed;
    runtime.pedestrianModelStats[model].pedType=StorylandBrawnZombie::pedestrianType;
    runtime.pedestrianModelStats[model].animationGroup=StorylandBrawnZombie::animationGroup;
    runtime.zombiePedestrians=true;
    error.clear();return true;
}

bool storylandLoadPlayTestCharacterSkin(const StorylandDtzArchive& archive,int32_t model,
    const std::string& name,StorylandPlayTestAsset& asset,std::string& error) {
    if(name.empty()||name.size()>8||!std::all_of(name.begin(),name.end(),[](unsigned char value) {
        return (value>='a'&&value<='z')||(value>='A'&&value<='Z')||
            (value>='0'&&value<='9')||value=='_';
    })) {
        error="Character skin requires its native eight-byte IMG name.";
        return false;
    }
    if(!storylandValidateSpecialModelAsset(archive,model,name,error))return false;
    std::vector<StorylandPlayTestAsset> candidates;
    if(!storylandLoadPlayTestModels(archive,{name},candidates,error)||candidates.size()!=1)return false;
    auto& candidate=candidates.front();
    if(candidate.model.modelKind()!=StorylandModelKind::PedModel||
        candidate.model.armatureBones().empty()||candidate.images.empty()) {
        error="Character skin requires decoded native pedestrian geometry, bones and textures.";
        return false;
    }
    asset=std::move(candidate);
    error.clear();return true;
}

bool storylandLoadPlayTestEscalatorStep(const StorylandDtzArchive& archive,uint32_t model,
    StorylandPlayTestAsset& asset,std::string& error) {
    uint32_t namedModel=UINT32_MAX;
    if(!archive.findModelIdByName("esc_step",namedModel)||namedModel!=model) {
        error="The native escalator step model does not match esc_step.";return false;
    }
    std::vector<StorylandPlayTestAsset> assets;
    if(storylandLoadPlayTestModels(archive,{"esc_step"},assets,error)&&!assets.empty()) {
        asset=std::move(assets.front());return true;
    }
    if (fatalPs2PacketError(error)) return false;
    StorylandPlayTestAsset resident;
    if(!resident.model.loadNativeSimpleModel(archive.unpackedBytes(),model,error,
        archive.gameVariant()==StorylandDtzGameVariant::Vcs)||
        resident.model.previewPoints().empty()||resident.model.previewTriangles().empty()) {
        error="Native esc_step geometry is unavailable: "+error;return false;
    }
    resident.name="esc_step";resident.materialColors=resident.model.previewMaterialColors();
    resident.geometryIdentity=populationGeometryIdentity(resident.model);
    assets.push_back(std::move(resident));
    storylandLoadPlayTestAssetTextures(archive,assets,error);
    asset=std::move(assets.front());error.clear();return true;
}
