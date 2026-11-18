#pragma once
#include <algorithm>
#include <array>
#include <filesystem>
#include <cctype>
#include <string>
#include <iterator>
#include <cstdint>

inline int storylandPoliceRadioChannel(bool vcs) {
    return vcs ? 31 : 23;
}

inline bool storylandPoliceRadioStream(const std::string& stem) {
    std::string name;
    for (unsigned char character : stem)
        if (std::isalnum(character)) name.push_back(char(std::tolower(character)));
    return name == "police";
}

inline double storylandRadioScriptPositionSeconds(int32_t position, bool vcs) {
    if (position < 0) return 0.0;
    // VCS script positions pass through 0x2FAE30 before the PS2 backend:
    // floor(position * 256 / 308), then floor(bytes * 7 / 256).
    const uint64_t bytes = (uint64_t(position) * 256u) / 308u;
    const uint64_t milliseconds = vcs ? (bytes * 7u) / 256u : uint64_t(position);
    return double(milliseconds) / 1000.0;
}

struct StorylandRadioBroadcast {
    struct Station {
        uint64_t milliseconds = 0;
        uint64_t savedTick = 0;
    };
    std::array<Station, 32> stations{};

    static uint64_t tick(uint32_t gameMilliseconds) {
        return uint64_t(gameMilliseconds) * 30u / 1000u;
    }

    void remember(int channel, uint64_t playbackMilliseconds, uint32_t gameMilliseconds) {
        if (channel >= 0 && size_t(channel) < stations.size())
            stations[size_t(channel)] = {playbackMilliseconds, tick(gameMilliseconds)};
    }

    double position(int channel, bool vcs, uint32_t gameMilliseconds, uint64_t vbBytes) const {
        if (channel < 0 || (channel >= (vcs ? 9 : 10) && channel != storylandPoliceRadioChannel(vcs)))
            return 0.0;
        const auto& station = stations[size_t(channel)];
        const uint64_t now = tick(gameMilliseconds);
        const uint64_t elapsed = now >= station.savedTick ? now - station.savedTick : now;
        // SLUS-21590 0x18B6A8 uses VB bytes. SLUS-21423 0x169FF0 uses milliseconds.
        if (vcs && vbBytes) {
            const uint64_t bytes = station.milliseconds * 256u / 7u + 1500u * elapsed + 100u;
            return double((bytes % vbBytes) * 7u / 256u) / 1000.0;
        }
        const uint64_t milliseconds = station.milliseconds +
            (vcs ? (1500u * elapsed + 100u) * 7u / 256u : 33u * elapsed + 500u);
        const uint64_t duration = vbBytes * 7u / 256u;
        return double(duration ? milliseconds % duration : milliseconds) / 1000.0;
    }

    double position(int channel, bool vcs, uint32_t gameMilliseconds,
        const std::filesystem::path& path) const {
        auto extension = path.extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return char(std::tolower(c)); });
        std::error_code error;
        const uint64_t bytes = extension == ".vb" ? std::filesystem::file_size(path, error) : 0;
        return position(channel, vcs, gameMilliseconds, error ? 0 : bytes);
    }
};

inline bool storylandIsRadioStation(const std::string& stem) {
    std::string name;
    for(unsigned char character:stem)
        if(std::isalnum(character))name.push_back(char(std::tolower(character)));
    static const char* stations[]={
        "head", "headradio", "lips", "lips106", "class", "classical", "double", "doubleclef",
        "doublecleffm", "rise", "risefm", "world", "radiodelmundo", "delmundo", "kjah", "kjahradio",
        "jam", "game", "gamefm", "libertyjam", "thelibertyjam", "msx", "msx98", "flash",
        "flashback", "flashbackfm", "chat", "lcfr", "libertycityfreeradio", "emotion", "emotion983", "fever",
        "fever105", "espant", "espantoso", "radioespantoso", "flashfm", "fresh", "fresh105", "freshfm",
        "paradise", "paradisefm", "vcfl", "vcpr", "vrock", "wave", "wave103", "classic",
        "msxfm", "kjahwest", "chatterbox",
    };
    return std::any_of(std::begin(stations),std::end(stations),[&](const char* station){return name==station;});
}

struct StorylandRadioStationInfo {
    std::string key, label;
};
inline const char* storylandRadioChannelKey(int channel,bool vcs) {
    // VCS retail debug station list, SLUS_215.90 strings at file offset 0x3C3FD8.
    static const char* vice[]={"flash","vrock","paradise","vcpr","vcfl","wave","fresh","espantoso","emotion"};
    static const char* liberty[]={"headradio","doubleclef","kjah","rise","lips106","delmundo","msx98","flashback","libertyjam","lcfr"};
    const int count=vcs?9:10;
    if(channel<0||channel>=count)return nullptr;
    return vcs?vice[channel]:liberty[channel];
}
inline int storylandRadioStationOrder(const std::string& key, bool vcs) {
    const int count = vcs ? 9 : 10;
    for (int channel = 0; channel < count; ++channel) {
        const char* nativeKey = storylandRadioChannelKey(channel, vcs);
        if (nativeKey && key == nativeKey) return channel;
    }
    return count;
}
inline StorylandRadioStationInfo storylandRadioStationInfo(const std::string& stem, bool vcs) {
    std::string name;
    for(unsigned char character:stem)
        if(std::isalnum(character))name.push_back(char(std::tolower(character)));
    struct Station {const char* key;const char* label;const char* aliases;};
    const Station lcs[]={
        {"headradio","Head Radio","|head|headradio|"},
        {"doubleclef","Double Clef FM","|class|classic|classical|double|doubleclef|doublecleffm|"},
        {"kjah","K-JAH","|kjah|kjahradio|"},
        {"rise","Rise FM","|rise|risefm|"},
        {"lips106","Lips 106","|lips|lips106|"},
        {"delmundo","Radio del Mundo","|world|delmundo|radiodelmundo|"},
        {"msx98","MSX 98","|msx|msx98|msxfm|"},
        {"flashback","Flashback FM","|flash|flashback|flashbackfm|"},
        {"libertyjam","The Liberty Jam","|jam|game|gamefm|libertyjam|thelibertyjam|"},
        {"lcfr","LCFR","|chat|lcfr|libertycityfreeradio|"}
    };
    const Station vice[]={
        {"wave","Wave 103","|wave|wave103|"},
        {"flash","Flash FM","|flash|flashfm|"},
        {"vrock","V-Rock","|vrock|"},
        {"vcfl","VCFL","|vcfl|"},
        {"paradise","Paradise FM","|paradise|paradisefm|"},
        {"espantoso","Radio Espantoso","|espant|espantoso|radioespantoso|"},
        {"emotion","Emotion 98.3","|emotion|emotion983|"},
        {"fresh","Fresh 105","|fresh|fresh105|freshfm|"},
        {"vcpr","VCPR","|vcpr|"}
    };
    const std::string alias="|"+name+"|";
    const Station* first=vcs?std::begin(vice):std::begin(lcs);
    const Station* last=vcs?std::end(vice):std::end(lcs);
    for(auto station=first;station!=last;++station)
        if(std::string(station->aliases).find(alias)!=std::string::npos)return {station->key,station->label};
    return {};
}
inline bool storylandRadioNonStation(const std::string& stem) {
    std::string name;
    for(unsigned char character:stem)
        if(std::isalnum(character))name.push_back(char(std::tolower(character)));
    return name=="city"||name=="water"||name=="ambience"||name=="ambient"||
        name=="frontend"||name=="intro"||name=="credits"||name=="loading"||
        name=="police"||name=="taxi";
}
