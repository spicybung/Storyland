#include "src/storyland/storyland_scm.h"
#include "src/storyland/storyland_gxt.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <limits>
#include <map>
#include <regex>
#include <sstream>

namespace {
#include "src/storyland/storyland_mission_titles.inc"

std::string storylandMissionTitleKey(std::string key) {
    const auto digit = key.find_last_not_of("0123456789");
    if (digit != std::string::npos && key.size() - digit == 2) key.insert(digit + 1, "0");
    return key;
}

std::string storylandReadableMissionTitle(const std::u16string& units) {
    std::string result;
    for (size_t index = 0; index < units.size(); ++index) {
        uint32_t codepoint = units[index];
        if (codepoint == '~') {
            const auto end = units.find(u'~', index + 1);
            if (end != std::u16string::npos) { index = end; continue; }
        }
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF && index + 1 < units.size() &&
            units[index + 1] >= 0xDC00 && units[index + 1] <= 0xDFFF) {
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + units[++index] - 0xDC00;
        } else if (codepoint >= 0xD800 && codepoint <= 0xDFFF) codepoint = 0xFFFD;
        if (codepoint < 0x80) result.push_back(char(codepoint));
        else if (codepoint < 0x800) {
            result.push_back(char(0xC0 | (codepoint >> 6)));
            result.push_back(char(0x80 | (codepoint & 63)));
        } else if (codepoint < 0x10000) {
            result.push_back(char(0xE0 | (codepoint >> 12)));
            result.push_back(char(0x80 | ((codepoint >> 6) & 63)));
            result.push_back(char(0x80 | (codepoint & 63)));
        } else {
            result.push_back(char(0xF0 | (codepoint >> 18)));
            result.push_back(char(0x80 | ((codepoint >> 12) & 63)));
            result.push_back(char(0x80 | ((codepoint >> 6) & 63)));
            result.push_back(char(0x80 | (codepoint & 63)));
        }
    }
    const auto begin = result.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    result = result.substr(begin, result.find_last_not_of(" \t\r\n") - begin + 1);
    if (result.size() >= 2 && result.front() == '\'' && result.back() == '\'')
        result = result.substr(1, result.size() - 2);
    return result == "REMOVED" ? std::string{} : result;
}

bool readWholeFileBinary(const std::wstring& filePath, std::vector<uint8_t>& bytes, std::string& errorMessage) {
    constexpr uint64_t kMaxScmBytes = 256ull * 1024ull * 1024ull;
    bytes.clear();
#ifdef _WIN32
    FILE* file = nullptr;
    if (_wfopen_s(&file, filePath.c_str(), L"rb") != 0 || file == nullptr) {
        errorMessage = "Could not open SCM file.";
        return false;
    }
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        fclose(file); errorMessage = "Could not seek SCM file."; return false;
    }
    const __int64 signedSize = _ftelli64(file);
    if (signedSize < 0 || uint64_t(signedSize) > kMaxScmBytes ||
        uint64_t(signedSize) > uint64_t((std::numeric_limits<size_t>::max)())) {
        fclose(file); errorMessage = "SCM file is too large to load safely."; return false;
    }
    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file); errorMessage = "Could not rewind SCM file."; return false;
    }
    bytes.resize(size_t(signedSize));
    if (!bytes.empty() && fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
        fclose(file); bytes.clear(); errorMessage = "Could not read the complete SCM file."; return false;
    }
    fclose(file);
    return true;
#else
    std::ifstream file(std::filesystem::path(filePath), std::ios::binary);
    if (!file) { errorMessage = "Could not open SCM file."; return false; }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0 || uint64_t(size) > kMaxScmBytes ||
        uint64_t(size) > uint64_t((std::numeric_limits<size_t>::max)()) ||
        uint64_t(size) > uint64_t((std::numeric_limits<std::streamsize>::max)())) {
        errorMessage = "SCM file is too large to load safely."; return false;
    }
    file.seekg(0, std::ios::beg);
    bytes.resize(size_t(size));
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()));
        if (size_t(file.gcount()) != bytes.size()) {
            bytes.clear(); errorMessage = "Could not read the complete SCM file."; return false;
        }
    }
    return true;
#endif
}

bool readWholeTextFile(const std::wstring& filePath, std::string& text, std::string& errorMessage) {
    std::vector<uint8_t> bytes;
    if (!readWholeFileBinary(filePath, bytes, errorMessage)) return false;

    size_t offset = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) offset = 3;
    if (offset >= bytes.size()) text.clear();
    else text.assign(reinterpret_cast<const char*>(bytes.data() + offset), bytes.size() - offset);
    return true;
}

uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 2 > bytes.size()) return 0;
    return uint16_t(bytes[offset]) | (uint16_t(bytes[offset + 1]) << 8);
}

uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0;
    return uint32_t(bytes[offset]) |
           (uint32_t(bytes[offset + 1]) << 8) |
           (uint32_t(bytes[offset + 2]) << 16) |
           (uint32_t(bytes[offset + 3]) << 24);
}

bool isHeaderGoto(const std::vector<uint8_t>& bytes, size_t offset) {
    return offset + 7 <= bytes.size() &&
           bytes[offset + 0] == 0x02 &&
           bytes[offset + 1] == 0x00 &&
           bytes[offset + 2] == 0x06;
}

bool decodeHeaderGotoTarget(const std::vector<uint8_t>& bytes, size_t offset, size_t& target) {
    if (!isHeaderGoto(bytes, offset)) return false;
    uint32_t stored = readU32(bytes, offset + 3);
    uint64_t actual = uint64_t(stored) + 8ull;
    if (actual > bytes.size()) return false;
    target = size_t(actual);
    return true;
}

std::string trimAscii(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

std::string lowerAscii(std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

struct SourceLine {
    size_t begin = 0;
    size_t end = 0;
    std::string text;
};

std::vector<SourceLine> splitSourceLines(const std::string& source) {
    std::vector<SourceLine> lines;
    size_t begin = 0;
    while (begin < source.size()) {
        size_t newline = source.find('\n', begin);
        size_t end = newline == std::string::npos ? source.size() : newline + 1;
        size_t textEnd = newline == std::string::npos ? source.size() : newline;
        if (textEnd > begin && source[textEnd - 1] == '\r') --textEnd;
        lines.push_back({begin, end, source.substr(begin, textEnd - begin)});
        begin = end;
    }
    if (source.empty()) lines.push_back({0, 0, std::string()});
    return lines;
}

std::string defaultMissionName(int id) {
    return "Unidentified script " + std::to_string(id);
}

bool readNativeScmIdentifier(const std::vector<uint8_t>& bytes, size_t& cursor,
    size_t end, bool vcs, std::string& value) {
    value.clear();
    if (vcs) {
        if (cursor >= end || bytes[cursor++] != 10) return false;
        while (cursor < end && bytes[cursor] && value.size() < 128)
            value.push_back(char(bytes[cursor++]));
        if (cursor >= end || bytes[cursor++] != 0) return false;
    } else {
        if (cursor > end || end - cursor < 8) return false;
        for (size_t index = 0; index < 8 && bytes[cursor + index]; ++index)
            value.push_back(char(bytes[cursor + index]));
        cursor += 8;
    }
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_';
    });
}

StorylandScmDialect identifyScmDialect(const std::vector<uint8_t>& bytes,
    size_t begin, size_t end) {
    end = std::min(end, bytes.size());
    StorylandScmDialect result = StorylandScmDialect::Unknown;
    for (bool vcs : {false, true}) {
        size_t cursor = begin;
        for (unsigned instruction = 0; instruction < 4 && cursor < end && end - cursor >= 2; ++instruction) {
            const uint16_t opcode = readU16(bytes, cursor);
            cursor += 2;
            if (opcode == 0 || opcode == (vcs ? 0x01EA : 0x031C)) continue;
            if (opcode != (vcs ? 0x0238 : 0x03A9) && opcode != (vcs ? 0x033A : 0x0551)) break;
            std::string identifier;
            if (!readNativeScmIdentifier(bytes, cursor, end, vcs, identifier)) break;
            const auto candidate = vcs ? StorylandScmDialect::Vcs : StorylandScmDialect::Lcs;
            if (result != StorylandScmDialect::Unknown && result != candidate)
                return StorylandScmDialect::Unknown;
            result = candidate;
            break;
        }
    }
    return result;
}

// Read only the native entry prologue; never search arbitrary operand bytes for opcodes.
void identifyMission(const std::vector<uint8_t>& bytes, bool vcs, StorylandScmMission& mission) {
    size_t cursor=mission.binaryOffset;
    const size_t end=std::min(bytes.size(),cursor+mission.binarySize);
    const uint16_t scriptName=vcs?0x0238:0x03A9, textTable=vcs?0x033A:0x0551;
    for(unsigned instruction=0;instruction<4 && cursor+2<=end;++instruction) {
        const uint16_t opcode=uint16_t(bytes[cursor])|(uint16_t(bytes[cursor+1])<<8);cursor+=2;
        if(opcode==0||opcode==(vcs?0x01EA:0x031C))continue;
        if(opcode!=scriptName&&opcode!=textTable)break;
        std::string value;
        if(!readNativeScmIdentifier(bytes,cursor,end,vcs,value))break;
        if(opcode==scriptName)mission.scriptName=value;else mission.textTable=value;
    }
    if(vcs&&mission.binarySize==24&&mission.binaryOffset+3<=bytes.size()&&
        bytes[mission.binaryOffset]==0xC6&&bytes[mission.binaryOffset+1]==3&&bytes[mission.binaryOffset+2]==1) {
        mission.name="Initialize bridge geometry";mission.category="Startup";
    }
    const auto& code=mission.scriptName.empty()?mission.textTable:mission.scriptName;
    if(!code.empty()) {mission.name=code;mission.category="Script";}
    if(!vcs&&code.find("INIT")==0)mission.category="Startup";
    if(vcs) {
        static const std::map<std::string,std::string> labels={
            {"INITPU","Initialize pickups"},{"INITCG","Initialize car generators"},
            {"INITGA","Initialize garages"},{"INITOB","Initialize objects"},
            {"INITPLA","Initialize player"},{"INITGEN","Initialize game settings"},
            {"BUILDDE","Empire developer menu"},{"BUILD","Empire building management"},
            {"PARAMED","Paramedic"},{"VIG","Vigilante"},{"FIRETRK","Firefighter"},
            {"DRIVERA","Driving activity"},{"TTRIAL","Time trial"},{"QTTRIAL","Quad time trial"},
            {"BTRIAL","Bicycle trial"},{"DTRIAL","Dirt bike trial"},{"GTRIAL","Vehicle trial"},
            {"CRASH","Crash activity"},{"BMXMALL","BMX mall challenge"},{"BEACHPL","Beach patrol"},
            {"TOURVCS","Tour activity"},{"FIREHEL","Fire helicopter activity"},
            {"RACEHUB","Race selector"},{"GYROTGT","Gyrocopter target challenge"},
            {"AIRRACE","Air race"},{"AIRRAC2","Air race 2"},{"MVRACE","Multivehicle race"},
            {"MVRACE2","Multivehicle race 2"},{"PCJPLAY","PCJ playground"},{"HOVERTT","Hovercraft time trial"},
            {"TAXI","Taxi driver"},{"SRANGE","Shooting range"},{"CHOPLIF","Chopper Checkpoint"},
            {"E_PR","Empire: Protection racket"},{"E_RE","Empire: Loan shark"},
            {"E_PI","Empire: Prostitution"},{"E_DR","Empire: Drugs"},
            {"E_SM","Empire: Smuggling"},{"E_RO","Empire: Robbery"}};
        const auto found=labels.find(code);
        if(found!=labels.end()) {mission.name=found->second;mission.category=code.find("INIT")==0?"Startup":"Activity";}
        if(code=="BUILDDE")mission.category="Development script";
    }
}

} // namespace

bool StorylandScmFile::loadFromFile(const std::wstring& filePath, std::string& errorMessage, bool loadNearbyText) {
    std::vector<uint8_t> bytes;
    if (!readWholeFileBinary(filePath, bytes, errorMessage)) return false;

    path = filePath;
    data = std::move(bytes);
    decompiledSource.clear();
    activeMode.clear();
    nativeHeader = {};
    objectNames.clear();
    nativeMissionRows.clear();
    missionRows.clear();
    parseNativeHeader();
    missionRows = nativeMissionRows;
    if(loadNearbyText) {
    const auto directory=std::filesystem::path(filePath).parent_path();
    const std::array<std::filesystem::path,3> textDirectories={directory,directory/L"TEXT",directory.parent_path()/L"TEXT"};
    bool titlesLoaded=false;
    for(const auto& folder:textDirectories) {
        for(const auto* name:{L"ENGLISH.GXT",L"english.gxt"}) {
            StorylandGxtFile text;std::string ignored;
            if(text.loadFromFile((folder/name).wstring(),ignored)) {applyRetailMissionTitles(text);titlesLoaded=true;break;}
        }
        if(titlesLoaded)break;
    }
    }
    errorMessage.clear();
    return true;
}

bool StorylandScmFile::setDecompiledSource(const std::string& text, const std::string& mode, std::string& errorMessage) {
    if (text.empty()) {
        errorMessage = "The decompiled SCM source is empty.";
        return false;
    }
    decompiledSource = text;
    activeMode = mode;
    parseMissions();
    errorMessage.clear();
    return true;
}

bool StorylandScmFile::replaceMissionSource(size_t missionIndex, const std::string& text, std::string& errorMessage) {
    if (!hasDecompiledSource()) {
        errorMessage = "No decompiled SCM source is loaded.";
        return false;
    }
    if (missionIndex >= missionRows.size()) {
        errorMessage = "The selected SCM mission index is out of range.";
        return false;
    }
    const StorylandScmMission selected = missionRows[missionIndex];
    if (selected.sourceEnd <= selected.sourceBegin || selected.sourceEnd > decompiledSource.size()) {
        errorMessage = "The selected mission does not have a source range to replace.";
        return false;
    }

    std::string replacement = text;
    if (!replacement.empty() && replacement.back() != '\n') replacement += "\r\n";
    decompiledSource.replace(selected.sourceBegin, selected.sourceEnd - selected.sourceBegin, replacement);
    parseMissions();
    errorMessage.clear();
    return true;
}

bool StorylandScmFile::applyMissionNamesFile(const std::wstring& filePath, std::string& errorMessage) {
    std::string text;
    if (!readWholeTextFile(filePath, text, errorMessage)) return false;

    std::vector<std::string> names;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        names.push_back(trimAscii(line));
    }

    auto apply = [&](std::vector<StorylandScmMission>& rows) {
        for (StorylandScmMission& mission : rows) {
            if (mission.id >= 0 && size_t(mission.id) < names.size() && !names[size_t(mission.id)].empty()) {
                mission.name = names[size_t(mission.id)];
            }
        }
    };
    apply(nativeMissionRows);
    apply(missionRows);
    errorMessage.clear();
    return true;
}

const std::wstring& StorylandScmFile::sourcePath() const {
    return path;
}

const std::vector<uint8_t>& StorylandScmFile::rawBytes() const {
    return data;
}

const std::string& StorylandScmFile::sourceText() const {
    return decompiledSource;
}

const std::string& StorylandScmFile::modeId() const {
    return activeMode;
}

const StorylandScmHeader& StorylandScmFile::header() const {
    return nativeHeader;
}

const std::vector<StorylandScmMission>& StorylandScmFile::missions() const {
    return missionRows;
}

bool StorylandScmFile::hasNativeMissionTable() const {
    return nativeHeader.valid && !nativeMissionRows.empty();
}

bool StorylandScmFile::hasDecompiledSource() const {
    return !decompiledSource.empty();
}

std::string StorylandScmFile::missionSource(size_t missionIndex) const {
    if (missionIndex >= missionRows.size()) return std::string();
    return missionRows[missionIndex].source;
}

std::vector<uint8_t> StorylandScmFile::missionBinary(size_t missionIndex) const {
    if (missionIndex >= missionRows.size()) return {};
    const StorylandScmMission& mission = missionRows[missionIndex];
    if (!mission.hasBinaryRange) return {};
    size_t begin = mission.binaryOffset;
    size_t end = begin + mission.binarySize;
    if (begin > data.size() || end > data.size() || end < begin) return {};
    return std::vector<uint8_t>(data.begin() + begin, data.begin() + end);
}

std::string StorylandScmFile::summaryLine() const {
    std::ostringstream ss;
    ss << "SCM " << data.size() << " bytes";
    if (nativeHeader.valid) {
        ss << " | native missions=" << nativeHeader.missionCount;
        if (nativeHeader.targetGame) ss << " | target=" << nativeHeader.targetGame;
    }
    if (!activeMode.empty()) ss << " | mode=" << activeMode;
    if (!decompiledSource.empty()) ss << " | source missions=" << missionRows.size();
    else if (!nativeHeader.valid) ss << " | unrecognized header";
    return ss.str();
}

void StorylandScmFile::parseNativeHeader() {
    nativeHeader = {};
    nativeMissionRows.clear();
    if (data.size() < 32) return;

    const uint32_t mainSizeMinus8 = readU32(data, 0);
    const uint32_t largestMission = readU32(data, 4);

    size_t secondGoto = 0;
    if (!decodeHeaderGotoTarget(data, 8, secondGoto)) return;
    if (secondGoto <= 15 || secondGoto + 8 > data.size()) return;

    const char targetGame = static_cast<char>(data[15]);
    if (targetGame != 'm' && targetGame != 'l') return;

    size_t thirdGoto = 0;
    if (!decodeHeaderGotoTarget(data, secondGoto, thirdGoto)) return;
    if (thirdGoto <= secondGoto + 7 || thirdGoto + 8 > data.size()) return;

    size_t cursor = secondGoto + 7;
    if (cursor + 1 + 4 > thirdGoto) return;
    cursor += 1;

    uint32_t saveVariableCount = readU32(data, cursor);
    cursor += 4;
    uint64_t saveBytes = uint64_t(saveVariableCount) * 2ull;
    if (saveBytes > (std::numeric_limits<size_t>::max)() || cursor + size_t(saveBytes) > thirdGoto) return;
    cursor += size_t(saveBytes);

    if (cursor + 4 > thirdGoto) return;
    uint32_t objectCount = readU32(data, cursor);
    cursor += 4;
    uint64_t objectBytes = uint64_t(objectCount) * 24ull;
    if (objectBytes > (std::numeric_limits<size_t>::max)() || cursor + size_t(objectBytes) > thirdGoto) return;
    objectNames.reserve(objectCount);
    for(uint32_t index=0;index<objectCount;++index) {
        const size_t begin=cursor+size_t(index)*24;size_t end=begin;
        while(end<begin+24&&data[end])++end;
        objectNames.emplace_back(reinterpret_cast<const char*>(data.data()+begin),end-begin);
    }
    cursor += size_t(objectBytes);

    if (cursor > thirdGoto) return;
    if (!isHeaderGoto(data, thirdGoto)) return;

    size_t mainScriptOffset = 0;
    if (!decodeHeaderGotoTarget(data, thirdGoto, mainScriptOffset)) return;
    if (mainScriptOffset <= thirdGoto + 7 || mainScriptOffset > data.size()) return;

    cursor = thirdGoto + 7;
    if (cursor + 1 + 2 + 2 + 4 + 2 + 2 > mainScriptOffset) return;
    cursor += 1;

    uint16_t trueGlobals = readU16(data, cursor);
    cursor += 2;
    uint16_t mostGlobals = readU16(data, cursor);
    cursor += 2;
    uint32_t largestMissionFromMultifile = readU32(data, cursor);
    cursor += 4;
    uint16_t missionCount = readU16(data, cursor);
    cursor += 2;
    uint16_t exclusiveMissionCount = readU16(data, cursor);
    cursor += 2;

    const size_t missionTableOffset = cursor;
    uint64_t missionTableBytes = uint64_t(missionCount) * 4ull;
    if (missionTableBytes > (std::numeric_limits<size_t>::max)() || cursor + size_t(missionTableBytes) > mainScriptOffset) return;

    std::vector<uint32_t> missionOffsets;
    missionOffsets.reserve(missionCount);
    for (uint16_t missionIndex = 0; missionIndex < missionCount; ++missionIndex) {
        uint32_t stored = readU32(data, cursor + size_t(missionIndex) * 4u);
        uint64_t actual = uint64_t(stored) + 8ull;
        if (actual > data.size()) return;
        missionOffsets.push_back(uint32_t(actual));
    }

    for (uint32_t offset : missionOffsets) {
        if (offset < mainScriptOffset || offset > data.size()) return;
    }

    uint64_t mainScriptSize64 = uint64_t(mainSizeMinus8) + 8ull;
    uint32_t mainScriptSize = mainScriptSize64 <= (std::numeric_limits<uint32_t>::max)()
        ? uint32_t(mainScriptSize64)
        : 0u;

    nativeHeader.valid = true;
    nativeHeader.targetGame = targetGame;
    nativeHeader.mainScriptSize = mainScriptSize;
    nativeHeader.largestMissionScriptSize = largestMissionFromMultifile != 0 ? largestMissionFromMultifile : largestMission;
    nativeHeader.mainScriptOffset = uint32_t(mainScriptOffset);
    nativeHeader.globalVariableBytes = uint32_t(secondGoto - 16);
    nativeHeader.saveVariableCount = saveVariableCount;
    nativeHeader.objectCount = objectCount;
    nativeHeader.trueGlobalCount = trueGlobals;
    nativeHeader.mostGlobalCount = mostGlobals;
    nativeHeader.missionCount = missionCount;
    nativeHeader.exclusiveMissionCount = exclusiveMissionCount;
    nativeHeader.missionTableOffset = uint32_t(missionTableOffset);

    nativeHeader.dialect = identifyScmDialect(data, mainScriptOffset,
        mainScriptSize > mainScriptOffset ? std::min<size_t>(mainScriptSize, data.size()) : data.size());
    if (nativeHeader.dialect == StorylandScmDialect::Unknown) {
        bool conflicting = false;
        for (uint32_t begin : missionOffsets) {
            size_t end = data.size();
            for (uint32_t candidate : missionOffsets)
                if (candidate > begin && candidate < end) end = candidate;
            const auto candidate = identifyScmDialect(data, begin, end);
            if (candidate == StorylandScmDialect::Unknown) continue;
            if (nativeHeader.dialect != StorylandScmDialect::Unknown && nativeHeader.dialect != candidate) {
                conflicting = true;
                break;
            }
            nativeHeader.dialect = candidate;
        }
        if (conflicting) nativeHeader.dialect = StorylandScmDialect::Unknown;
    }

    nativeMissionRows.reserve(missionOffsets.size());
    for (size_t index = 0; index < missionOffsets.size(); ++index) {
        uint32_t begin = missionOffsets[index];
        uint32_t end = uint32_t(data.size());
        for (uint32_t candidate : missionOffsets) {
            if (candidate > begin && candidate < end) end = candidate;
        }
        if (end < begin) end = begin;

        StorylandScmMission mission;
        mission.id = int(index);
        mission.name = defaultMissionName(mission.id);
        mission.binaryOffset = begin;
        mission.binarySize = end - begin;
        mission.hasBinaryRange = true;
        if(nativeHeader.dialect!=StorylandScmDialect::Unknown)
            identifyMission(data,nativeHeader.dialect==StorylandScmDialect::Vcs,mission);
        nativeMissionRows.push_back(std::move(mission));
    }
    applyRetailMissionTitles(StorylandGxtFile{});
}

void StorylandScmFile::parseMissions() {
    missionRows = nativeMissionRows;
    if (decompiledSource.empty()) return;

    const std::regex definePattern(
        R"(^\s*DEFINE\s+MISSION\s+([0-9]+)\s+AT\s+@([A-Za-z0-9_.$]+))",
        std::regex_constants::icase
    );
    const std::regex labelPattern(
        R"(^\s*:([A-Za-z0-9_.$]+))",
        std::regex_constants::icase
    );

    struct MissionDeclaration {
        int id = -1;
        std::string label;
    };

    std::vector<MissionDeclaration> declarations;
    std::map<std::string, size_t> labelOffsets;
    const std::vector<SourceLine> lines = splitSourceLines(decompiledSource);

    for (const SourceLine& line : lines) {
        std::smatch match;
        if (std::regex_search(line.text, match, definePattern)) {
            MissionDeclaration declaration;
            try {
                declaration.id = std::stoi(match[1].str());
            } catch (...) {
                declaration.id = -1;
            }
            declaration.label = match[2].str();
            if (declaration.id >= 0 && !declaration.label.empty()) declarations.push_back(std::move(declaration));
        }

        if (std::regex_search(line.text, match, labelPattern)) {
            std::string label = lowerAscii(match[1].str());
            if (labelOffsets.find(label) == labelOffsets.end()) labelOffsets[label] = line.begin;
        }
    }

    struct LocatedMission {
        MissionDeclaration declaration;
        size_t begin = 0;
    };

    std::vector<LocatedMission> located;
    for (const MissionDeclaration& declaration : declarations) {
        auto found = labelOffsets.find(lowerAscii(declaration.label));
        if (found == labelOffsets.end()) continue;
        located.push_back({declaration, found->second});
    }

    std::sort(located.begin(), located.end(), [](const LocatedMission& a, const LocatedMission& b) {
        if (a.begin != b.begin) return a.begin < b.begin;
        return a.declaration.id < b.declaration.id;
    });

    for (size_t i = 0; i < located.size(); ++i) {
        const size_t begin = located[i].begin;
        const size_t end = i + 1 < located.size() ? located[i + 1].begin : decompiledSource.size();
        const int id = located[i].declaration.id;

        auto existing = std::find_if(missionRows.begin(), missionRows.end(), [&](const StorylandScmMission& row) {
            return row.id == id;
        });
        if (existing == missionRows.end()) {
            StorylandScmMission mission;
            mission.id = id;
            mission.name = defaultMissionName(id);
            missionRows.push_back(std::move(mission));
            existing = missionRows.end() - 1;
        }

        existing->label = located[i].declaration.label;
        existing->sourceBegin = begin;
        existing->sourceEnd = end;
        existing->source = decompiledSource.substr(begin, end - begin);
    }

    std::sort(missionRows.begin(), missionRows.end(), [](const StorylandScmMission& a, const StorylandScmMission& b) {
        return a.id < b.id;
    });
}

void StorylandScmFile::applyRetailMissionTitles(const StorylandGxtFile& text) {
    std::map<std::string,std::string> titles;
    for(size_t index=0;index<text.entries().size();++index) {
        const auto& entry=text.entries()[index];
        if(entry.table>=text.tables().size()||text.tables()[entry.table].name!="MAIN")continue;
        titles.emplace(entry.key,storylandReadableMissionTitle(text.text(index)));
    }
    auto apply=[&](auto& rows) {
        for(auto& row:rows) {
            std::string key=row.textTable.empty()?row.scriptName:row.textTable;
            if(key.empty())continue;
            if(nativeHeader.dialect==StorylandScmDialect::Vcs) {
                const auto catalog=std::find_if(std::begin(storylandVcsMissionTitles),std::end(storylandVcsMissionTitles),
                    [&](const auto& entry) {
                        return storylandMissionTitleKey(entry.identity)==storylandMissionTitleKey(key) ||
                            (!row.scriptName.empty() &&
                                storylandMissionTitleKey(entry.identity)==storylandMissionTitleKey(row.scriptName));
                    });
                if(catalog!=std::end(storylandVcsMissionTitles)) {
                    std::string resolved;
                    std::istringstream keys(catalog->textKeys);
                    std::string alias;
                    bool complete=true;
                    while(std::getline(keys,alias,'|')) {
                        const auto found=titles.find(alias);
                        if(found==titles.end()||found->second.empty()) {complete=false;break;}
                        if(!resolved.empty())resolved+=" / ";
                        resolved+=found->second;
                    }
                    row.name=complete&&!resolved.empty()?resolved:catalog->english;
                    row.category=catalog->category;
                    continue;
                }
                key=storylandMissionTitleKey(key);
            }
            auto found=titles.find(key);
            if(found!=titles.end()&&!found->second.empty()) {row.name=found->second;if(row.category!="Activity")row.category="Mission";}
        }
    };
    apply(nativeMissionRows);apply(missionRows);
}
