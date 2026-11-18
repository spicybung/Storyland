#pragma once
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_sdt.h"
#include "src/storyland/storyland_vag_bank.h"
#include "src/storyland/storyland_vehicle_audio.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>

struct StorylandNativeSoundSample {
    std::wstring bankPath;
    size_t slot = 0;
    std::string label;
    std::string key;
    uint32_t frequency = 0;
    int nativeVcsBank = -1;
    int nativeLcsBank = -1;
    bool psp = false;
    uint32_t missionNameHash = 0;
    bool hasMissionNameHash = false;
};

inline std::string storylandSoundKey(const std::string& text) {
    std::string result;
    for (unsigned char character : text) {
        if (std::isalnum(character)) result.push_back(char(std::tolower(character)));
    }
    return result;
}

inline uint32_t storylandMissionAudioNameHash(const std::string& name) {
    uint32_t hash = 0xFFFFFFFFu;
    for (unsigned char character : name) {
        hash ^= uint32_t(std::toupper(character));
        for (unsigned bit = 0; bit < 8; ++bit)
            hash = (hash >> 1) ^ ((hash & 1u) ? 0xEDB88320u : 0u);
    }
    return hash;
}

inline bool storylandReadMissionAudioLookup(const std::filesystem::path& bankPath,
    size_t sampleCount, std::vector<uint32_t>& hashes, std::string& error) {
    hashes.clear();
    std::filesystem::path lookup;
    auto folder = bankPath.parent_path();
    for (unsigned level = 0; level < 2 && !folder.empty() && lookup.empty(); ++level) {
        std::error_code filesystemError;
        std::filesystem::directory_iterator cursor(folder,
            std::filesystem::directory_options::skip_permission_denied, filesystemError), end;
        for (; !filesystemError && cursor != end; cursor.increment(filesystemError)) {
            std::error_code entryError;
            auto filename = cursor->path().filename().string();
            std::transform(filename.begin(), filename.end(), filename.begin(),
                [](unsigned char character) { return char(std::tolower(character)); });
            if (cursor->is_regular_file(entryError) && !entryError &&
                filename == "m_lookup.bin") {
                lookup = cursor->path();
                break;
            }
        }
        const auto parent = folder.parent_path();
        if (parent == folder) break;
        folder = parent;
    }
    if (lookup.empty()) {
        error = "The VCS mission audio bank requires its original M_LOOKUP.BIN.";
        return false;
    }
    std::ifstream input(lookup, std::ios::binary | std::ios::ate);
    const auto length = input ? input.tellg() : std::streampos(-1);
    if (!sampleCount || sampleCount > 100000 || length < 0 ||
        uint64_t(length) != uint64_t(sampleCount) * 4u) {
        error = "The VCS M_LOOKUP.BIN entry count does not match the mission audio bank.";
        return false;
    }
    std::vector<uint8_t> bytes(sampleCount * 4u);
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()))) {
        error = "The VCS mission audio lookup could not be read.";
        return false;
    }
    std::vector<uint32_t> decoded;
    decoded.reserve(sampleCount);
    std::set<uint32_t> identities;
    for (size_t slot = 0; slot < sampleCount; ++slot) {
        const size_t offset = slot * 4u;
        const uint32_t hash = uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8) |
            (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
        if (!identities.insert(hash).second) {
            error = "The VCS mission audio lookup contains an ambiguous name hash.";
            return false;
        }
        decoded.push_back(hash);
    }
    hashes = std::move(decoded);
    error.clear();
    return true;
}

inline std::string storylandWeaponSoundFamily(const std::string& name) {
    const auto key = storylandSoundKey(name);
    if (key == "pistol") return "colt45";
    if (key == "revolver") return "python";
    if (key == "m4" || key == "m16") return "ruger";
    if (key == "rocketlauncher" || key == "rlaunch" || key == "rpg") return "rocket";
    if (key == "mp5lng") return "mp5";
    if (key == "tec9" || key == "ingram" || key == "mac10") return "tec";
    if (key == "stubby" || key == "sawnoff" || key == "chromegun") return "shotgun";
    if (key == "laserscope" || key == "laser") return "sniper";
    return key;
}

class StorylandNativeAudioCatalog {
public:
    std::vector<StorylandNativeSoundSample> samples;

    void clear() { samples.clear(); }

    size_t addBank(const std::wstring& path, const StorylandSdtIndex& index,bool vcs=false) {
        if (index.stride != 12) return 0;
        auto raw = std::filesystem::path(path);
        raw.replace_extension(L".RAW");
        if (!std::filesystem::is_regular_file(raw)) raw.replace_extension(L".raw");
        std::ifstream input(raw, std::ios::binary);
        if (!input) return 0;
        int nativeBank=-1,nativeLcsBank=-1,vcsBankNumber=-1;
        const std::string stem=storylandSoundKey(std::filesystem::path(path).stem().string());
        if(vcs&&stem.size()>5&&stem.compare(0,3,"sfx")==0&&stem.compare(stem.size()-2,2,"ps")==0) {
            unsigned number=0;bool valid=true;
            for(size_t i=3;i<stem.size()-2;++i) {
                if(stem[i]<'0'||stem[i]>'9'||number>67) {valid=false;break;}
                number=number*10+unsigned(stem[i]-'0');
            }
            if(valid&&number<=67) {
                vcsBankNumber=int(number);
                if((number<40||(number>=61&&number<=66))&&
                    index.samples.size()==storylandVcsBankSampleCount(number))
                    nativeBank=int(number);
            }
        }
        if(!vcs) {
            if(stem=="sfx0ps"&&index.samples.size()==313)nativeLcsBank=0;
            else if(stem=="sfx2ps"&&index.samples.size()==5201)nativeLcsBank=2;
        }
        std::vector<uint32_t> missionHashes;
        std::string lookupError;
        if(vcsBankNumber==67&&!storylandReadMissionAudioLookup(std::filesystem::path(path),
            index.samples.size(),missionHashes,lookupError))return 0;
        const size_t before = samples.size();
        for (size_t slot = 0; slot < index.samples.size(); ++slot) {
            const auto& sample = index.samples[slot];
            if (sample.length < 48) continue;
            std::array<char, 48> header{};
            input.clear();
            input.seekg(std::streamoff(sample.offset));
            if (!input.read(header.data(), header.size())) continue;
            if (std::string(header.data(), 4) != "VAGp") continue;
            std::string label;
            for (size_t offset = 32; offset < 48 && header[offset]; ++offset) {
                const auto character = static_cast<unsigned char>(header[offset]);
                if (character < 32 || character > 126) { label.clear(); break; }
                label.push_back(char(character));
            }
            if (!label.empty()||nativeBank>=0||nativeLcsBank>=0||!missionHashes.empty()) {
                samples.push_back({path,slot,label,storylandSoundKey(label),sample.frequency,nativeBank,nativeLcsBank});
                if(!missionHashes.empty()) {
                    samples.back().missionNameHash=missionHashes[slot];
                    samples.back().hasMissionNameHash=true;
                }
            }
        }
        return samples.size() - before;
    }

    size_t addPspBank(const std::wstring& path, std::string& error) {
        error.clear();
        const int bank = storylandPspVagBankNumber(std::filesystem::path(path));
        if (bank < 0) return 0;
        std::vector<StorylandVagBankSample> decoded;
        if (!storylandReadVagBankIndex(std::filesystem::path(path), decoded, error)) return 0;
        std::vector<uint32_t> missionHashes;
        if (bank == 67 && !storylandReadMissionAudioLookup(std::filesystem::path(path),
            decoded.size(), missionHashes, error)) return 0;
        const int nativeBank = bank == 0 && decoded.size() == 349 ? 0 :
            bank > 0 && bank < 40 && decoded.size() == storylandVcsBankSampleCount(unsigned(bank)) ? bank : -1;
        const size_t before = samples.size();
        for (size_t slot = 0; slot < decoded.size(); ++slot) {
            const auto& sample = decoded[slot];
            samples.push_back({path, slot, sample.name, storylandSoundKey(sample.name),
                sample.frequency, nativeBank, -1, true});
            if (!missionHashes.empty()) {
                samples.back().missionNameHash = missionHashes[slot];
                samples.back().hasMissionNameHash = true;
            }
        }
        error.clear();
        return samples.size() - before;
    }

    const StorylandNativeSoundSample* missionSample(const std::string& name) const {
        const auto key = storylandSoundKey(name);
        if (key.empty() || name.find_first_of("/\\:") != std::string::npos) return nullptr;
        const uint32_t hash = storylandMissionAudioNameHash(name);
        const uint32_t fallbackHash = storylandMissionAudioNameHash("RENC1AA");
        const StorylandNativeSoundSample* fallback = nullptr;
        bool ps2MissionLookup = false;
        for (const auto& sample : samples) {
            if (!sample.hasMissionNameHash) continue;
            if (sample.missionNameHash == hash) return &sample;
            if (!sample.psp) {
                ps2MissionLookup = true;
                if (sample.missionNameHash == fallbackHash) fallback = &sample;
            }
        }
        // Original VCS PS2 resolver 0x1C65F0 retries RENC1AA after a missing lookup hash.
        if (ps2MissionLookup) return fallback;
        const StorylandNativeSoundSample* result = nullptr;
        for (const auto& sample : samples) {
            if (sample.hasMissionNameHash || sample.key != key) continue;
            if (sample.label.find_first_of("/\\:") != std::string::npos) return nullptr;
            if (!result) result = &sample;
        }
        return result;
    }

    std::vector<const StorylandNativeSoundSample*> matching(const std::vector<std::string>& prefixes) const {
        std::vector<const StorylandNativeSoundSample*> result;
        for (const auto& sample : samples) {
            for (const auto& prefix : prefixes) {
                if (sample.key.compare(0, prefix.size(), prefix) == 0) {
                    result.push_back(&sample);
                    break;
                }
            }
        }
        return result;
    }

    const StorylandNativeSoundSample* vehicleDoorSound(const StorylandVehicleDefinition& vehicle,
        bool opening,bool psp=false) const {
        if(vehicle.twoWheels()||(vehicle.vcsModelLayout&&vehicle.nativeType==9))return nullptr;
        uint8_t type=vehicle.doorSoundType;
        if(vehicle.vcsModelLayout) {
            if(!vehicle.hasNativeAudio)return nullptr;
            const uint32_t slot=psp?storylandVcsPspVehicleDoorSample(type,opening):
                storylandVcsVehicleDoorSample(type,opening);
            if(const auto* sample=vcsSample(0,slot,psp))return sample;
        } else {
            if(!storylandLcsVehicleDoorType(vehicle.modelId,type))return nullptr;
            if(!psp)
                if(const auto* sample=lcsSample(storylandLcsVehicleDoorSample(type,opening)))return sample;
        }
        const char* names[][2]={
            {"sfxoldcardooropen","sfxoldcardoorclose"},
            {"sfxnewcardooropen","sfxnewcardoorclose"},
            {"sfxtruckdooropen","sfxtruckdoorclose"},
            {"sfxairbrakes","sfxairbrakes"}
        };
        if(type>=4)return nullptr;
        const auto* sample=namedVehicleSample(names[type][opening?0:1]);
        return sample&&sample->psp==psp?sample:nullptr;
    }

    const StorylandNativeSoundSample* vehicleStartSound(const StorylandVehicleDefinition& vehicle) const {
        if (vehicle.bicycle()) return nullptr;
        if (vehicle.motorcycle()) {
            for (const char* name : {"sfxbikestarter", "sfxmotorcyclestart", "sfxmopedstarter"}) {
                if (const auto* sample = namedVehicleSample(name)) return sample;
            }
            return nullptr;
        }
        return namedVehicleSample("sfxcarstarter");
    }

    std::vector<const StorylandNativeSoundSample*> event(StorylandPlayTestSound type,
        const std::string& weapon = {}, const std::string& material = {}) const {
        const auto family = storylandWeaponSoundFamily(weapon);
        const auto nativeWeapon=storylandSoundKey(weapon);
        if(!nativeWeapon.empty()&&type==StorylandPlayTestSound::WeaponShot) {
            auto exact=matching({"sfx"+nativeWeapon+"left","sfx"+nativeWeapon+"right","sfx"+nativeWeapon+"fire"});
            if(!exact.empty())return exact;
        }
        if(!nativeWeapon.empty()&&type==StorylandPlayTestSound::WeaponReload) {
            auto exact=matching({"sfx"+nativeWeapon+"reload"});
            if(!exact.empty())return exact;
        }
        switch (type) {
        case StorylandPlayTestSound::Footstep:
            if (material == "water") return matching({"sfxfootstepwat", "footstepwat"});
            if (material == "wood") return matching({"sfxfootstepwoo", "footstepwoo"});
            if (material == "metal") return matching({"sfxfootstepmet", "footstepmet"});
            if (material == "sand") return matching({"sfxfootstepsan", "footstepsan"});
            if (material == "granular") return matching({"sfxfootstepgra", "footstepgra"});
            return matching({"sfxfootstepcon", "footstepcon"});
        case StorylandPlayTestSound::Land:
            if(const auto* sample=namedVehicleSample("sfxbodyland"))return {sample};
            return {};
        case StorylandPlayTestSound::VehicleEnter:
        case StorylandPlayTestSound::VehicleExit:
        case StorylandPlayTestSound::VehicleDoorOpen:
        case StorylandPlayTestSound::VehicleDoorClose:
            // The caller must supply the actual vehicle's door sound type.
            return {};
        case StorylandPlayTestSound::Damage: return matching({"malepain"});
        case StorylandPlayTestSound::WaterSplash: return matching({"sfxsplash", "sfxboatsplash"});
        case StorylandPlayTestSound::Melee:
            if(nativeWeapon.find("knife")!=std::string::npos || nativeWeapon.find("blade")!=std::string::npos ||
                nativeWeapon.find("katana")!=std::string::npos) {
                auto blade=matching({"sfxknifeslash", "sfxknifestab"});
                if(!blade.empty())return blade;
            }
            if(nativeWeapon.find("bat")!=std::string::npos || nativeWeapon.find("golf")!=std::string::npos) {
                auto blunt=matching({"sfxbathit", "bathit"});
                if(!blunt.empty())return blunt;
            }
            return matching({"punch13", "punch14", "kickbody", "dynhitsbody"});
        case StorylandPlayTestSound::MeleeSwing:
            // Exact original VCS SFX0 bank labels verified against retail
            // SFX0_PSP.RAW, not guesses about generic sound filenames.
            if(nativeWeapon.find("knife")!=std::string::npos || nativeWeapon.find("blade")!=std::string::npos ||
                nativeWeapon.find("katana")!=std::string::npos) {
                auto blade=matching({"sfxknifeswing"});
                if(!blade.empty())return blade;
            }
            if(nativeWeapon.find("golf")!=std::string::npos) {
                auto golf=matching({"sfxgolfclubsw"});
                if(!golf.empty())return golf;
            }
            // The four retail FIGHT samples are the common unarmed/club
            // swing family; PUNCH13/PUNCH14 are strictly contact impacts.
            return matching({"sfxfight1", "sfxfight2", "sfxfight4", "sfxfight5"});
        case StorylandPlayTestSound::WeaponPickup: return matching({"sfxweaponleft", "sfxweaponcentr", "sfxweaponright"});
        case StorylandPlayTestSound::Explosion: return matching({"sfxexplosion"});
        case StorylandPlayTestSound::VehicleImpact: return matching({"sfxcolcar"});
        case StorylandPlayTestSound::VehicleStarter: return {};
        case StorylandPlayTestSound::RadioSwitch: {
            // The dial samples contain the brief tuning noise; the click on its
            // own is too short to represent changing stations.
            auto dial = matching({"sfxradiodial"});
            return dial.empty() ? matching({"sfxradioclick"}) : dial;
        }
        case StorylandPlayTestSound::WeaponShot:
            if (family.empty()) return {};
            return matching({"sfx" + family + "left", "sfx" + family + "right", "sfx" + family + "fire"});
        case StorylandPlayTestSound::WeaponReload:
            if (family == "ak47") return matching({"sfxak47reload"});
            if (family == "rocket") return matching({"sfxrocketreloa"});
            if (family == "ruger" || family == "sniper" || family == "m60") return matching({"sfxriflereload"});
            if (family == "colt45" || family == "python" || family == "mp5" || family == "tec" || family == "uzi")
                return matching({"sfxpistolreloa"});
            return {};
        default: return {};
        }
    }

    std::vector<const StorylandNativeSoundSample*> characterReaction(bool female,bool fatal) const {
        const std::string prefix=std::string(female?"female":"male")+(fatal?"death":"pain");
        return matching({prefix});
    }

    bool verifiesVcsSpeechBank(const std::wstring& path) const {
        unsigned anchors=0;
        for(const auto& sample:samples)if(sample.bankPath==path) {
            if(sample.slot==0&&sample.key=="crash1")++anchors;
            if(sample.slot==44&&sample.key=="bump1")++anchors;
            if(sample.slot==3116&&sample.key=="bustedcalm1")++anchors;
        }
        return anchors==3;
    }

    const StorylandNativeSoundSample* vcsSample(unsigned bank, size_t slot, bool psp) const {
        for (const auto& sample : samples)
            if (sample.nativeVcsBank == int(bank) && sample.psp == psp && sample.slot == slot) return &sample;
        return nullptr;
    }

    const StorylandNativeSoundSample* vcsSample(unsigned bank,size_t slot) const {
        for(const auto& sample:samples)
            if(sample.nativeVcsBank==int(bank)&&(!sample.psp||bank!=0)&&sample.slot==slot)return &sample;
        if (bank != 0) return nullptr;
        // Shared mission/vehicle tables use PS2 IDs. Resolve their named PSP
        // equivalents, never interpret a PS2 index as a slot in the larger bank.
        struct Alias { size_t ps2Slot; const char* prefix; };
        static constexpr Alias aliases[] = {
            {4, "shopbell"}, {6, "sfxarmbomb"}, {12, "beep"},
            {20, "sfxboatwaterl"}, {23, "sfxbombbeep"},
            {31, "sfxbulletshell"}, {33, "sfxbulletwall"},
            {37, "sfxcardboardbo"},
            {43, "sfxcarhorn56c"}, {44, "sfxcarhornbmw"},
            {47, "sfxcarhornjee"}, {48, "sfxcarhornpic"},
            {49, "sfxcarhornpor"}, {50, "sfxcarhorntru"},
            {51, "sfxcaridlecad"}, {52, "sfxcaridlecob"},
            {53, "golfcartidle"}, {54, "sfxcaridlehon"},
            {55, "sfxcaridlehot"}, {56, "sfxcaridlemer"},
            {57, "sfxcaridlepac"}, {58, "sfxcaridlepat"},
            {60, "sfxcaridlepor"}, {61, "sfxcaridlespi"},
            {62, "sfxcaridlespo"}, {64, "sfxcaridletru"},
            {66, "sfxcaridlevtw"}, {68, "sfxcaronfire"},
            {73, "sfxcarrevcadi"}, {74, "sfxcarrevcobr"},
            {75, "sfxcarrevgolf"}, {76, "sfxcarrevhond"},
            {77, "hovcrzferfrom"}, {78, "sfxcarrevhotr"},
            {79, "sfxcarrevmerc"}, {80, "sfxcarrevpaca"},
            {81, "sfxcarrevpath"}, {83, "sfxcarrevpors"},
            {84, "sfxcarrevspid"}, {85, "sfxcarrevspor"},
            {87, "sfxcarrevtruc"}, {89, "sfxcarrevvtwi"},
            {90, "sfxcarstarter"}, {132, "cuttingrigging"},
            {162, "sfxgaragedoor"}, {211, "sfxmopedidle"},
            {212, "sfxmopedrev"}, {260, "sfxskid"},
            {272, "sfxtankturret"}, {291, "sfxwoodenboxs"}
        };
        const auto alias = std::find_if(std::begin(aliases), std::end(aliases),
            [&](const Alias& value) { return value.ps2Slot == slot; });
        if (alias == std::end(aliases)) return nullptr;
        const StorylandNativeSoundSample* found = nullptr;
        const size_t length = std::char_traits<char>::length(alias->prefix);
        for (const auto& sample : samples) {
            if (!sample.psp || sample.nativeVcsBank != 0 || sample.key.compare(0, length, alias->prefix) != 0) continue;
            if (found) return nullptr;
            found = &sample;
        }
        return found;
    }

    const StorylandNativeSoundSample* vcsVehicleSample(size_t slot) const {
        for (const auto& sample : samples)
            if (sample.nativeVcsBank == 0 && sample.psp && sample.slot == slot) return &sample;
        return vcsSample(0, slot);
    }

    const StorylandNativeSoundSample* vcsVehicleSample(size_t slot, bool psp) const {
        for (const auto& sample : samples)
            if (sample.nativeVcsBank == 0 && sample.psp == psp && sample.slot == slot) return &sample;
        return nullptr;
    }

    std::array<const StorylandNativeSoundSample*,2> pspVehicleEnginePair(
        const StorylandVehicleDefinition& vehicle) const {
        const auto* rev = vcsVehicleSample(vehicle.accelerationSample);
        if (!rev || !rev->psp) return {nullptr, nullptr};
        struct Pair { const char* rev; const char* idle; };
        static constexpr Pair pairs[] = {
            {"sfxcarrevcadi", "sfxcaridlecad"}, {"sfxcarrevcobr", "sfxcaridlecob"},
            {"sfxcarrevgolf", "golfcartidle"}, {"sfxcarrevhond", "sfxcaridlehon"},
            {"sfxcarrevhotr", "sfxcaridlehot"}, {"sfxcarrevmerc", "sfxcaridlemer"},
            {"sfxcarrevpaca", "sfxcaridlepac"}, {"sfxcarrevpath", "sfxcaridlepat"},
            {"sfxcarrevpont", "sfxcaridlepon"}, {"sfxcarrevpors", "sfxcaridlepor"},
            {"sfxcarrevspid", "sfxcaridlespi"}, {"sfxcarrevspor", "sfxcaridlespo"},
            {"sfxcarrevtruc", "sfxcaridletru"}, {"sfxcarrevvtwi", "sfxcaridlevtw"},
            {"sfxmopedrev", "sfxmopedidle"}
        };
        for (const auto& pair : pairs) {
            if (rev->key.compare(0, std::char_traits<char>::length(pair.rev), pair.rev) != 0) continue;
            for (const auto& sample : samples) {
                if (sample.bankPath == rev->bankPath &&
                    sample.key.compare(0, std::char_traits<char>::length(pair.idle), pair.idle) == 0)
                    return {&sample, rev};
            }
        }
        return {nullptr, rev};
    }

    const StorylandNativeSoundSample* lcsSample(uint32_t sampleId) const {
        const int bank=sampleId<313?0:sampleId>=314&&sampleId<5515?2:-1;
        if(bank<0)return nullptr;
        const size_t slot=bank==0?sampleId:sampleId-314;
        for(const auto& sample:samples)
            if(sample.nativeLcsBank==bank&&sample.slot==slot)return &sample;
        return nullptr;
    }

    const StorylandNativeSoundSample* lcsPoliceSample(uint32_t sampleId) const {
        return lcsSample(sampleId);
    }


    std::array<const StorylandNativeSoundSample*,2> enginePair(
        const StorylandVehicleDefinition& vehicle,bool water,bool playerVehicle=true) const {
        if(water)return enginePair(true,false);
        if(vehicle.vcsModelLayout&&vehicle.hasNativeAudio) {
            const auto* nativeRev = vcsVehicleSample(vehicle.accelerationSample);
            if (nativeRev && nativeRev->psp) return pspVehicleEnginePair(vehicle);
            uint32_t idle=0,rev=0;
            if(storylandVcsEngineSamples(vehicle.audioBank,idle,rev))
                return {vcsSample(0,idle),vcsSample(0,playerVehicle?rev:vehicle.accelerationSample)};
            // Special vehicle engines use separate processing, never a random car bank.
            return {nullptr,nullptr};
        }
        return enginePair(water,vehicle.motorcycle(),vehicle.vcsModelLayout,vehicle.modelId);
    }

    const StorylandNativeSoundSample* namedVehicleSample(const std::string& key) const {
        const StorylandNativeSoundSample* truncated = nullptr;
        bool ambiguous = false;
        for (const auto& sample : samples) {
            if (sample.key == key) return &sample;
            if ((sample.label.size() == 16 || (sample.psp && sample.label.size() >= 12)) &&
                sample.key.size() >= 10 && sample.key.size() < key.size() &&
                key.compare(0, sample.key.size(), sample.key) == 0) {
                if (truncated) ambiguous = true;
                else truncated = &sample;
            }
        }
        return ambiguous ? nullptr : truncated;
    }

    const StorylandNativeSoundSample* vehicleSirenSample(const StorylandVehicleDefinition& vehicle,
        bool psp) const {
        if(vehicle.vcsModelLayout)
            return vehicle.hasNativeAudio?vcsVehicleSample(vehicle.sirenSample,psp):nullptr;
        const auto siren=storylandLcsVehicleSirenAudio(vehicle.modelId);
        if(siren.sample==UINT32_MAX)return nullptr;
        if(const auto* native=lcsSample(siren.sample))return native;
        return siren.name?namedVehicleSample(siren.name):nullptr;
    }

    std::array<const StorylandNativeSoundSample*,2> enginePair(bool water,bool motorcycle,
        bool vcs,uint32_t modelId) const {
        if (!vcs && !water) if (const auto* profile = storylandLcsVehicleAudio(modelId))
            return {namedVehicleSample(profile->idle), namedVehicleSample(profile->rev)};
        return enginePair(water,motorcycle);
    }

    std::array<const StorylandNativeSoundSample*,2> enginePair(bool water,bool motorcycle) const {
        std::array<const StorylandNativeSoundSample*,2> result{engine(water,motorcycle,false),nullptr};
        if(!result[0]) {result[1]=engine(water,motorcycle,true);return result;}
        const std::string& idle=result[0]->key;
        const size_t marker=idle.find("idle");
        if(marker!=std::string::npos) {
            const std::string expected=idle.substr(0,marker)+"rev"+idle.substr(marker+4);
            for(const auto& sample:samples) {
                if(sample.bankPath!=result[0]->bankPath)continue;
                // VAG names are limited to sixteen characters; accept a shared truncated suffix.
                const size_t length=std::min(expected.size(),sample.key.size());
                if(length>=10&&expected.compare(0,length,sample.key,0,length)==0) {
                    result[1]=&sample;break;
                }
            }
        }
        if(!result[1])result[1]=engine(water,motorcycle,true);
        return result;
    }

    const StorylandNativeSoundSample* vehicleNoise(bool horn) const {
        const auto found=horn?matching({"sfxcarhorn","sfxhorn","carhorn"}):
            matching({"sfxskid","sfxcarskid","sfxroadski","tyreskid","tireskid"});
        return found.empty()?nullptr:found.front();
    }

    const StorylandNativeSoundSample* engine(bool water, bool motorcycle, bool revving) const {
        std::vector<std::string> prefixes;
        if (water) prefixes = revving ? std::vector<std::string>{"sfxboatv12loo"} :
            std::vector<std::string>{"sfxboatcruiser", "sfxfishingboat"};
        else if (motorcycle) prefixes = revving ? std::vector<std::string>{"sfxmopedrev", "sfxcarrevvtwi"} :
            std::vector<std::string>{"sfxmopedidle", "sfxcaridlevtw"};
        else prefixes = revving ? std::vector<std::string>{"sfxcarrev"} : std::vector<std::string>{"sfxcaridle"};
        const auto found = matching(prefixes);
        return found.empty() ? nullptr : found.front();
    }
};
