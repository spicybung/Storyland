#include "storyland_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

static uint32_t modelReadU32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) | (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

static uint16_t modelReadU16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
}

static int16_t modelReadI16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    uint16_t raw = uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
    return static_cast<int16_t>(raw);
}

static float modelReadF32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0.0f;
    float value = 0.0f;
    std::memcpy(&value, data.data() + offset, 4);
    return value;
}

static std::string modelHex32(uint32_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return ss.str();
}

static std::string modelHexOffset(size_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << value;
    return ss.str();
}

static bool sanePreviewFloat(float value) {
    return std::isfinite(value) && value >= -64.0f && value <= 64.0f;
}

static const char* knownMdlHeaderName(size_t offset) {
    switch (offset) {
    case 0x00: return "sChunkHeader.ident";
    case 0x04: return "sChunkHeader.shrink_or_unknown";
    case 0x08: return "sChunkHeader.fileSize";
    case 0x0C: return "sChunkHeader.localReallocTable";
    case 0x10: return "sChunkHeader.globalReallocTable";
    case 0x14: return "sChunkHeader.relocationEntryCount";
    case 0x18: return "sChunkHeader.ptr2BeforeTexNameList";
    case 0x1C: return "sChunkHeader.allocatedMemory";
    case 0x20: return "payload.slot0 / colModel_or_elementGroup";
    case 0x24: return "payload.slot1 / elementGroup_or_type";
    default: return "sChunkHeader/payload dword";
    }
}

static bool modelPointerLooksValid(const std::vector<uint8_t>& data, uint32_t value) {
    return value >= 0x20 && value + 4 <= data.size() && (value % 4) == 0;
}

static std::string modelLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}

static bool modelLooksLikeTextureName(const std::string& value) {
    if (value.size() < 3 || value.size() > 64) return false;

    bool hasUnderscore = false;
    bool hasLetter = false;
    bool hasBadChar = false;
    for (char c : value) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc >= 127) {
            hasBadChar = true;
            break;
        }
        if (std::isalpha(uc)) hasLetter = true;
        if (c == '_') hasUnderscore = true;
        if (!(std::isalnum(uc) || c == '_' || c == '-' || c == '.')) {
            hasBadChar = true;
            break;
        }
    }
    if (hasBadChar || !hasLetter) return false;

    std::string lower = modelLowerAscii(value);
    static const char* rejectedExact[] = {
        "root", "pivots", "pelvis", "spine", "spine1", "neck", "head", "jaw", "l_calf", "r_calf",
        "l_foot", "r_foot", "l_toe0", "r_toe0", "l_hand", "r_hand", "l_finger", "r_finger",
        "scene_root", "male_base", "female_base", "object", "clump", "atomic", "geometry"
    };
    for (const char* rejected : rejectedExact) {
        if (lower == rejected) return false;
    }

    if (lower.find("bip01") != std::string::npos) return false;
    if (lower.find("dummy") != std::string::npos) return false;
    if (lower.find("bone") != std::string::npos) return false;
    if (lower.find("frame") != std::string::npos) return false;

    return hasUnderscore || lower.find("tex") != std::string::npos || lower.find("skin") != std::string::npos || lower.find("head") != std::string::npos || lower.find("body") != std::string::npos;
}

static std::string modelFileNameFromPath(const std::wstring& filePath) {
    size_t slash = filePath.find_last_of(L"\\/");
    std::wstring wideName = (slash == std::wstring::npos) ? filePath : filePath.substr(slash + 1);
    std::string name;
    name.reserve(wideName.size());
    for (wchar_t ch : wideName) name.push_back(ch >= 0 && ch <= 127 ? char(ch) : '?');
    return modelLowerAscii(name);
}

static bool modelContainsAsciiLower(const std::vector<uint8_t>& data, const char* needle) {
    const size_t needleLength = std::strlen(needle);
    if (needleLength == 0 || data.size() < needleLength) return false;

    for (size_t offset = 0; offset + needleLength <= data.size(); ++offset) {
        bool matched = true;
        for (size_t index = 0; index < needleLength; ++index) {
            unsigned char ch = data[offset + index];
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<unsigned char>(ch - 'A' + 'a');
            if (ch != static_cast<unsigned char>(needle[index])) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}

static uint32_t modelCountAsciiHints(const std::vector<uint8_t>& data, const std::vector<const char*>& hints) {
    uint32_t count = 0;
    for (const char* hint : hints) {
        if (modelContainsAsciiLower(data, hint)) count++;
    }
    return count;
}

static std::string classifySectionId(uint32_t value) {
    if (value == 0xAAAAAAAAu) return "";
    if (value == 0x0000AA02u || value == 0x00000002u) return "Clump";
    if (value == 0x0004AA01u || value == 0x0000AA01u || value == 0x01050001u || ((value & 0xFFFF00FFu) == 0x00040001u)) return "Atomic";
    if (value == 0x0180AA00u || value == 0x0003AA00u || value == 0x0C000000u || ((value & 0xFF0000FFu) == 0x01000000u)) return "RslNode1";
    if (value == 0x0380AA00u || value == 0x0003AA80u || value == 0x0380AA80u || ((value & 0xFF0000FFu) == 0x03000000u)) return "RslNode2";
    if (value == 0x0003AA01u || value == 0x0003AA02u || value == 0x0380AA01u || value == 0x0380AA02u) return "RslNode3";
    return "";
}

static std::string sectionValueBytes(uint32_t value) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0')
       << std::setw(2) << ((value >> 0) & 0xFF) << ' '
       << std::setw(2) << ((value >> 8) & 0xFF) << ' '
       << std::setw(2) << ((value >> 16) & 0xFF) << ' '
       << std::setw(2) << ((value >> 24) & 0xFF);
    return ss.str();
}

static std::string mdlSectionMeaning(uint32_t value) {
    switch (value) {
    case 0x0000AA02u: return "RslElementGroup / Clump";
    case 0x00000002u: return "LCS RslElementGroup / Clump";
    case 0x0004AA01u: return "RslElement / Atomic";
    case 0x01050001u: return "LCS/PSP RslElement / Atomic";
    case 0x0000AA01u: return "RslElement / Atomic variant";
    case 0x0180AA00u: return "RslNode1 / Frame";
    case 0x0C000000u: return "LCS RslNode / Frame";
    case 0x0003AA00u: return "RslNode1 / Frame variant";
    case 0x0380AA00u: return "RslNode2 / Frame with hierarchy/name extension";
    case 0x0003AA80u: return "RslNode2 / Frame variant";
    case 0x0380AA80u: return "RslNode2 / Frame variant";
    case 0x0003AA01u:
    case 0x0003AA02u:
    case 0x0380AA01u:
    case 0x0380AA02u: return "RslNode3 / Geometry-frame extension candidate";
    default: return "";
    }
}

static void addField(std::vector<StorylandModelField>& fields, const std::string& group, const std::string& name, uint32_t offset, uint32_t value, const std::string& note);

static void addMatrixFields(const std::vector<uint8_t>& data, std::vector<StorylandModelField>& fields, const std::string& group, uint32_t matrixOffset, const std::string& prefix) {
    static const char* rowNames[4] = {"right", "up", "at", "pos"};
    for (uint32_t row = 0; row < 4; ++row) {
        uint32_t base = matrixOffset + row * 0x10;
        if (base + 0x10 > data.size()) break;
        std::ostringstream note;
        note << prefix << '.' << rowNames[row] << " = ("
             << modelReadF32(data, base + 0x00) << ", "
             << modelReadF32(data, base + 0x04) << ", "
             << modelReadF32(data, base + 0x08) << ", "
             << modelReadF32(data, base + 0x0C) << ")";
        addField(fields, group, std::string(prefix) + "." + rowNames[row], base, modelReadU32(data, base), note.str());
    }
}

static bool looksLikeClumpAt(const std::vector<uint8_t>& data, uint32_t offset) {
    if (!modelPointerLooksValid(data, offset)) return false;
    uint32_t value = modelReadU32(data, offset);
    return value == 0x0000AA02u || value == 0x00000002u;
}

static bool looksLikeAtomicAt(const std::vector<uint8_t>& data, uint32_t offset) {
    if (!modelPointerLooksValid(data, offset)) return false;
    uint32_t value = modelReadU32(data, offset);
    return value == 0x0004AA01u || value == 0x0000AA01u || value == 0x01050001u || ((value & 0xFFFF00FFu) == 0x00040001u);
}

static void addField(std::vector<StorylandModelField>& fields, const std::string& group, const std::string& name, uint32_t offset, uint32_t value, const std::string& note) {
    fields.push_back({group, name, offset, value, note});
}

static void addPointerField(const std::vector<uint8_t>& data, std::vector<StorylandModelField>& fields, const std::string& group, const std::string& name, uint32_t offset, const std::string& note) {
    uint32_t value = modelReadU32(data, offset);
    std::string fullNote = note;
    if (modelPointerLooksValid(data, value)) {
        if (!fullNote.empty()) fullNote += "; ";
        fullNote += "valid file offset -> first dword " + modelHex32(modelReadU32(data, value));
    }
    addField(fields, group, name, offset, value, fullNote);
}

void StorylandModelFile::detectModelKind() {
    kind = StorylandModelKind::Unknown;
    if (data.size() < 0x28) return;

    // Some Stories/Leeds vehicle-family resources are not wrapped in the normal
    // ldm relocatable chunk.  coquette.mdl is an example: it begins with
    // bytes "M G 00 00" and contains a compact raw geometry stream.  Do not
    // send these through the PED/Cutscene armature path or the normal ldm
    // Clump/Atomic resolver.
    if (data.size() >= 4 && data[0] == 'M' && data[1] == 'G' && data[2] == 0 && data[3] == 0) {
        kind = StorylandModelKind::VehicleModel;
        return;
    }

    uint32_t collisionPointer = modelReadU32(data, 0x20);
    uint32_t clumpPointer = modelReadU32(data, 0x24);

    bool hasCollision = modelPointerLooksValid(data, collisionPointer);
    bool hasClump = modelPointerLooksValid(data, clumpPointer);

    uint32_t atomicCount = 0;
    uint32_t clumpCount = 0;
    uint32_t rslNodeCount = 0;
    uint32_t rslNode2Count = 0;
    uint32_t aaTaggedCount = 0;

    for (size_t offset = 0; offset + 4 <= data.size(); offset += 4) {
        uint32_t value = modelReadU32(data, offset);
        if (value == 0x0004AA01u || value == 0x01050001u || ((value & 0xFFFF00FFu) == 0x00040001u)) atomicCount++;
        if (value == 0x0000AA02u || value == 0x00000002u) clumpCount++;
        if (value == 0x0003AA00u || value == 0x0380AA00u || value == 0x0003AA80u || value == 0x0380AA80u || value == 0x0C000000u || ((value & 0xFF0000FFu) == 0x01000000u) || ((value & 0xFF0000FFu) == 0x03000000u)) rslNodeCount++;
        if (value == 0x0380AA00u || value == 0x0003AA80u || value == 0x0380AA80u) rslNode2Count++;
        if ((value & 0x0000FF00u) == 0x0000AA00u || (value & 0x00FF0000u) == 0x00AA0000u) aaTaggedCount++;
    }

    static const std::vector<const char*> pedHints = {
        "scene_root", "pelvis", "spine", "spine1", "neck", "head", "jaw",
        "upperarm", "forearm", "hand", "thigh", "calf", "foot", "male_base", "female_base"
    };

    static const std::vector<const char*> vehicleHints = {
        "chassis", "wheel", "bonnet", "boot", "door", "bump_front", "bump_rear",
        "windscreen", "exhaust", "ped_frontseat", "ped_backseat", "wing_l", "wing_r"
    };

    uint32_t pedNameHits = modelCountAsciiHints(data, pedHints);
    uint32_t vehicleNameHits = modelCountAsciiHints(data, vehicleHints);

    std::string lowerFileName = modelFileNameFromPath(path);
    bool cutsceneNameHint = lowerFileName.rfind("cs", 0) == 0 || lowerFileName.find("/cs") != std::string::npos || lowerFileName.find("\\cs") != std::string::npos;

    bool skeletalByNames = pedNameHits >= 3;
    bool skeletalBySections = rslNodeCount >= 2 && rslNode2Count >= 1 && atomicCount >= 1 && clumpCount >= 1;
    bool vehicleByNames = vehicleNameHits >= 3;
    bool vehicleBySections = atomicCount >= 3 && rslNodeCount >= 3 && vehicleNameHits >= 1;

    if (vehicleByNames || vehicleBySections) kind = StorylandModelKind::VehicleModel;
    else if (cutsceneNameHint && (skeletalByNames || rslNodeCount >= 4)) kind = StorylandModelKind::CutsceneModel;
    else if (skeletalByNames || skeletalBySections) kind = StorylandModelKind::PedModel;
    else if (cutsceneNameHint) kind = StorylandModelKind::CutsceneModel;
    else if (hasClump || hasCollision || atomicCount >= 1 || clumpCount >= 1 || aaTaggedCount >= 1) kind = StorylandModelKind::SimpleModel;
    else kind = StorylandModelKind::Unknown;
}

bool StorylandModelFile::loadFromFile(const std::wstring& filePath, std::string& errorMessage) {
    path = filePath;
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        errorMessage = "Could not open MDL file.";
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 0) {
        errorMessage = "Could not determine MDL file size.";
        return false;
    }
    data.resize(size_t(size));
    if (!data.empty()) file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file && size != 0) {
        errorMessage = "Could not read full MDL file.";
        return false;
    }
    parse();
    return true;
}



struct StorylandPreviewTransform {
    bool valid = false;
    float sx = 1.0f;
    float sy = 1.0f;
    float sz = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;
    float tz = 0.0f;
};

static bool previewFiniteReasonable(float value, float limit);

struct StorylandGeometryPartEntry {
    uint32_t stripOffset = 0;
    uint32_t materialIndex = 0xFFFFFFFFu;
};

struct StorylandGeometryLayout {
    bool valid = false;
    uint32_t atomicOffset = 0;
    uint32_t geometryOffset = 0;
    uint32_t geoStart = 0;
    uint32_t materialCount = 0;
    StorylandPreviewTransform transform;
    std::vector<std::string> materialTextureNames;
    std::vector<StorylandGeometryPartEntry> parts;
};

static std::string readModelCStringAt(const std::vector<uint8_t>& data, uint32_t offset, size_t maxLength = 96) {
    if (offset >= data.size()) return std::string();
    std::string value;
    for (size_t index = 0; index < maxLength && offset + index < data.size(); ++index) {
        unsigned char ch = data[size_t(offset) + index];
        if (ch == 0) break;
        if (ch < 32 || ch >= 127) return std::string();
        value.push_back(char(ch));
    }
    return value;
}


static bool modelPointLooksReasonable(const StorylandModelPoint& point, float limit) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) && point.x >= -limit && point.x <= limit && point.y >= -limit && point.y <= limit && point.z >= -limit && point.z <= limit;
}

static StorylandModelPoint readModelMatrixPosition(const std::vector<uint8_t>& data, uint32_t matrixOffset) {
    StorylandModelPoint point;
    point.x = modelReadF32(data, size_t(matrixOffset) + 0x30);
    point.y = modelReadF32(data, size_t(matrixOffset) + 0x34);
    point.z = modelReadF32(data, size_t(matrixOffset) + 0x38);
    return point;
}

struct StorylandModelMatrix {
    std::array<float, 16> m = {};
    bool valid = false;
};

static StorylandModelMatrix identityModelMatrix() {
    StorylandModelMatrix matrix;
    matrix.valid = true;
    matrix.m[0] = 1.0f;
    matrix.m[5] = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
    return matrix;
}

static StorylandModelMatrix readModelMatrix(const std::vector<uint8_t>& data, uint32_t matrixOffset) {
    StorylandModelMatrix matrix;
    if (size_t(matrixOffset) + 0x40 > data.size()) return matrix;
    for (size_t index = 0; index < 16; ++index) {
        float value = modelReadF32(data, size_t(matrixOffset) + index * 4u);
        if (!std::isfinite(value) || value < -10000.0f || value > 10000.0f) return StorylandModelMatrix{};
        matrix.m[index] = value;
    }
    matrix.valid = true;
    return matrix;
}

static StorylandModelMatrix multiplyModelMatrix(const StorylandModelMatrix& parent, const StorylandModelMatrix& local) {
    if (!parent.valid) return local;
    if (!local.valid) return parent;

    StorylandModelMatrix result;
    result.valid = true;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            result.m[size_t(column) * 4u + size_t(row)] =
                parent.m[0 * 4 + row] * local.m[size_t(column) * 4u + 0] +
                parent.m[1 * 4 + row] * local.m[size_t(column) * 4u + 1] +
                parent.m[2 * 4 + row] * local.m[size_t(column) * 4u + 2] +
                parent.m[3 * 4 + row] * local.m[size_t(column) * 4u + 3];
        }
    }
    return result;
}

static StorylandModelPoint matrixModelPosition(const StorylandModelMatrix& matrix) {
    return StorylandModelPoint{matrix.m[12], matrix.m[13], matrix.m[14]};
}

static bool matrixModelRotationToQuat(const StorylandModelMatrix& matrix, float& x, float& y, float& z, float& w) {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    w = 1.0f;

    if (!matrix.valid) return false;

    // RSL/Rw matrices here are stored as right/up/at/pos vectors.
    // Treat right/up/at as the 3x3 basis columns.
    float m00 = matrix.m[0];
    float m01 = matrix.m[4];
    float m02 = matrix.m[8];

    float m10 = matrix.m[1];
    float m11 = matrix.m[5];
    float m12 = matrix.m[9];

    float m20 = matrix.m[2];
    float m21 = matrix.m[6];
    float m22 = matrix.m[10];

    float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        if (s <= 0.00001f) return false;
        w = 0.25f * s;
        x = (m21 - m12) / s;
        y = (m02 - m20) / s;
        z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        if (s <= 0.00001f) return false;
        w = (m21 - m12) / s;
        x = 0.25f * s;
        y = (m01 + m10) / s;
        z = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        if (s <= 0.00001f) return false;
        w = (m02 - m20) / s;
        x = (m01 + m10) / s;
        y = 0.25f * s;
        z = (m12 + m21) / s;
    } else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        if (s <= 0.00001f) return false;
        w = (m10 - m01) / s;
        x = (m02 + m20) / s;
        y = (m12 + m21) / s;
        z = 0.25f * s;
    }

    float length = std::sqrt(x * x + y * y + z * z + w * w);
    if (!std::isfinite(length) || length < 0.00001f) {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 1.0f;
        return false;
    }

    x /= length;
    y /= length;
    z /= length;
    w /= length;
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
}

static StorylandModelPoint pointAdd(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return StorylandModelPoint{a.x + b.x, a.y + b.y, a.z + b.z};
}

static StorylandModelPoint pointSub(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return StorylandModelPoint{a.x - b.x, a.y - b.y, a.z - b.z};
}

static StorylandModelPoint pointMul(const StorylandModelPoint& a, float scale) {
    return StorylandModelPoint{a.x * scale, a.y * scale, a.z * scale};
}

static const std::vector<std::string>& canonicalPedBoneNames() {
    static const std::vector<std::string> names = {
        "scene_root", "pivots", "male_base", "pelvis", "spine", "spine1", "neck", "head", "jaw",
        "l_clavicle", "l_upperarm", "l_forearm", "l_hand", "l_finger",
        "r_clavicle", "r_upperarm", "r_forearm", "r_hand", "r_finger",
        "l_thigh", "l_calf", "l_foot", "l_toe0",
        "r_thigh", "r_calf", "r_foot", "r_toe0"
    };
    return names;
}

static bool isKnownPedBoneName(const std::string& lower) {
    const auto& names = canonicalPedBoneNames();
    for (const std::string& name : names) {
        if (lower == name) return true;
    }

    if (lower == "female_base") return true;
    if (lower == "root") return true;
    if (lower == "base") return true;
    if (lower.find("pelvis") != std::string::npos) return true;
    if (lower.find("spine") != std::string::npos) return true;
    if (lower.find("head") != std::string::npos) return true;
    if (lower.find("jaw") != std::string::npos) return true;
    if (lower.find("neck") != std::string::npos) return true;
    if (lower.find("upperarm") != std::string::npos) return true;
    if (lower.find("forearm") != std::string::npos) return true;
    if (lower.find("clavicle") != std::string::npos) return true;
    if (lower.find("thigh") != std::string::npos) return true;
    if (lower.find("calf") != std::string::npos) return true;
    if (lower.find("foot") != std::string::npos) return true;
    if (lower.find("toe") != std::string::npos) return true;
    if (lower.find("finger") != std::string::npos) return true;
    if (lower == "l_hand" || lower == "r_hand") return true;
    return false;
}

struct StorylandModelNameHit {
    uint32_t offset = 0;
    std::string name;
};

static std::vector<StorylandModelNameHit> collectPedBoneNameHits(const std::vector<uint8_t>& data) {
    std::vector<StorylandModelNameHit> hits;
    std::set<std::string> seenExact;

    for (size_t offset = 0; offset < data.size();) {
        unsigned char ch = data[offset];
        if (ch < 32 || ch >= 127) {
            ++offset;
            continue;
        }

        size_t start = offset;
        std::string value;
        while (offset < data.size()) {
            unsigned char c = data[offset];
            if (c == 0) break;
            if (c < 32 || c >= 127) break;
            value.push_back(char(c));
            ++offset;
            if (value.size() > 64) break;
        }

        if (offset < data.size() && data[offset] == 0 && value.size() >= 2 && value.size() <= 64) {
            std::string lower = modelLowerAscii(value);
            if (isKnownPedBoneName(lower)) {
                std::string key = std::to_string(start) + ":" + lower;
                if (seenExact.insert(key).second) {
                    hits.push_back({uint32_t(start), lower});
                }
            }
        }

        offset = std::max(offset + 1, start + 1);
    }

    std::sort(hits.begin(), hits.end(), [](const StorylandModelNameHit& a, const StorylandModelNameHit& b) {
        return a.offset < b.offset;
    });
    return hits;
}

static bool isLeedsDmaPacketWord(uint32_t value) {
    return (value & 0xFF000000u) == 0x60000000u;
}

static bool findLeedsGeometryLayout(const std::vector<uint8_t>& data, StorylandGeometryLayout& layout) {
    layout = StorylandGeometryLayout{};

    for (size_t atomicOffset = 0; atomicOffset + 0x38 <= data.size(); atomicOffset += 4) {
        uint32_t sectionId = modelReadU32(data, atomicOffset);
        if (sectionId != 0x0004AA01u && sectionId != 0x0000AA01u && sectionId != 0x01050001u && ((sectionId & 0xFFFF00FFu) != 0x00040001u)) continue;

        uint32_t geometryPointer = modelReadU32(data, atomicOffset + 0x14);
        if (!modelPointerLooksValid(data, geometryPointer)) continue;
        if (geometryPointer + 0x60 > data.size()) continue;

        uint32_t materialListPointer = modelReadU32(data, geometryPointer + 0x0C);
        uint32_t materialCount = modelReadU32(data, geometryPointer + 0x10);
        if (materialCount == 0 || materialCount > 512) continue;
        if (!modelPointerLooksValid(data, materialListPointer)) continue;
        if (materialListPointer + materialCount * 4u > data.size()) continue;

        StorylandGeometryLayout candidate;
        candidate.valid = true;
        candidate.atomicOffset = uint32_t(atomicOffset);
        candidate.geometryOffset = geometryPointer;
        candidate.materialCount = materialCount;

        candidate.materialTextureNames.reserve(materialCount);
        for (uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
            uint32_t materialPointer = modelReadU32(data, materialListPointer + materialIndex * 4u);
            std::string textureName;
            if (modelPointerLooksValid(data, materialPointer) && materialPointer + 0x10 <= data.size()) {
                uint32_t textureNamePointer = modelReadU32(data, materialPointer + 0x00);
                textureName = readModelCStringAt(data, textureNamePointer);
            }
            candidate.materialTextureNames.push_back(modelLowerAscii(textureName));
        }

        float sx = modelReadF32(data, geometryPointer + 0x48);
        float sy = modelReadF32(data, geometryPointer + 0x4C);
        float sz = modelReadF32(data, geometryPointer + 0x50);
        float tx = modelReadF32(data, geometryPointer + 0x54);
        float ty = modelReadF32(data, geometryPointer + 0x58);
        float tz = modelReadF32(data, geometryPointer + 0x5C);

        if (previewFiniteReasonable(sx, 1000.0f) && previewFiniteReasonable(sy, 1000.0f) && previewFiniteReasonable(sz, 1000.0f) &&
            std::fabs(sx) >= 0.000001f && std::fabs(sy) >= 0.000001f && std::fabs(sz) >= 0.000001f &&
            previewFiniteReasonable(tx, 1000.0f) && previewFiniteReasonable(ty, 1000.0f) && previewFiniteReasonable(tz, 1000.0f)) {
            candidate.transform.valid = true;
            candidate.transform.sx = sx;
            candidate.transform.sy = sy;
            candidate.transform.sz = sz;
            candidate.transform.tx = tx;
            candidate.transform.ty = ty;
            candidate.transform.tz = tz;
        }

        size_t partTableOffset = size_t(geometryPointer) + 0x60u;
        size_t rowOffset = partTableOffset;
        const size_t maxPartRows = 2048;
        for (size_t rowIndex = 0; rowIndex < maxPartRows && rowOffset + 0x30 <= data.size(); ++rowIndex, rowOffset += 0x30) {
            uint32_t markerOrTemp = modelReadU32(data, rowOffset);
            if (isLeedsDmaPacketWord(markerOrTemp)) {
                candidate.geoStart = uint32_t(rowOffset);
                break;
            }

            uint32_t stripOffset = modelReadU32(data, rowOffset + 0x1C);
            uint16_t materialIndex = uint16_t(modelReadI16(data, rowOffset + 0x22));
            if (stripOffset < data.size() && materialIndex < materialCount) {
                candidate.parts.push_back({stripOffset, materialIndex});
            }
        }

        if (candidate.geoStart == 0) {
            for (size_t scan = partTableOffset; scan + 0x14 <= data.size(); scan += 4) {
                uint32_t value = modelReadU32(data, scan);
                if (isLeedsDmaPacketWord(value) && modelReadU32(data, scan + 0x10) == 0x6C018000u) {
                    candidate.geoStart = uint32_t(scan);
                    break;
                }
            }
        }

        if (candidate.geoStart == 0) continue;
        if (candidate.parts.empty()) candidate.parts.push_back({0, 0});

        std::sort(candidate.parts.begin(), candidate.parts.end(), [](const StorylandGeometryPartEntry& a, const StorylandGeometryPartEntry& b) {
            if (a.stripOffset != b.stripOffset) return a.stripOffset < b.stripOffset;
            return a.materialIndex < b.materialIndex;
        });
        candidate.parts.erase(std::unique(candidate.parts.begin(), candidate.parts.end(), [](const StorylandGeometryPartEntry& a, const StorylandGeometryPartEntry& b) {
            return a.stripOffset == b.stripOffset && a.materialIndex == b.materialIndex;
        }), candidate.parts.end());

        layout = std::move(candidate);
        return true;
    }

    return false;
}

static bool previewFiniteReasonable(float value, float limit) {
    return std::isfinite(value) && value >= -limit && value <= limit;
}

static StorylandPreviewTransform findLeedsPreviewTransform(const std::vector<uint8_t>& data) {
    StorylandGeometryLayout layout;
    if (findLeedsGeometryLayout(data, layout)) return layout.transform;
    return StorylandPreviewTransform{};
}

static StorylandModelPoint decodeLeedsPackedPosition(
    int16_t rawX,
    int16_t rawY,
    int16_t rawZ,
    const StorylandPreviewTransform& transform
) {
    if (!transform.valid) {
        return StorylandModelPoint{float(rawX), float(rawY), float(rawZ)};
    }

    const float globalScale = 100.0f * 0.00000030518203134641490805874367518203f;
    return StorylandModelPoint{
        float(rawX) * transform.sx * globalScale + transform.tx,
        float(rawY) * transform.sy * globalScale + transform.ty,
        float(rawZ) * transform.sz * globalScale + transform.tz
    };
}

static float distanceSquared(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return dx * dx + dy * dy + dz * dz;
}

static float triangleAreaSquared(const StorylandModelPoint& a, const StorylandModelPoint& b, const StorylandModelPoint& c) {
    float abX = b.x - a.x;
    float abY = b.y - a.y;
    float abZ = b.z - a.z;
    float acX = c.x - a.x;
    float acY = c.y - a.y;
    float acZ = c.z - a.z;

    float crossX = abY * acZ - abZ * acY;
    float crossY = abZ * acX - abX * acZ;
    float crossZ = abX * acY - abY * acX;

    return crossX * crossX + crossY * crossY + crossZ * crossZ;
}

static bool blockHasShape(const std::vector<StorylandModelPoint>& blockPoints) {
    if (blockPoints.size() < 3) return false;

    float minX = blockPoints[0].x;
    float minY = blockPoints[0].y;
    float minZ = blockPoints[0].z;
    float maxX = blockPoints[0].x;
    float maxY = blockPoints[0].y;
    float maxZ = blockPoints[0].z;

    for (const StorylandModelPoint& point : blockPoints) {
        minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
        minZ = std::min(minZ, point.z); maxZ = std::max(maxZ, point.z);
    }

    float spanX = std::fabs(maxX - minX);
    float spanY = std::fabs(maxY - minY);
    float spanZ = std::fabs(maxZ - minZ);

    if (!std::isfinite(spanX) || !std::isfinite(spanY) || !std::isfinite(spanZ)) return false;
    return (spanX + spanY + spanZ) > 0.0001f;
}

static void appendTriangleStripPreview(
    uint32_t baseVertex,
    size_t vertexCount,
    uint32_t materialIndex,
    const std::vector<StorylandModelPoint>& allPoints,
    std::vector<StorylandModelTriangle>& triangles
) {
    if (vertexCount < 3) return;

    // Leeds PS2 strips use duplicate/zero-area triangles as strip breaks.
    // The old preview only skipped the zero-area faces but kept the original
    // running parity.  After a restart marker that flips the winding of every
    // following face, which makes rigid SimpleModels like ak47 look shredded
    // even when all vertices were decoded.
    size_t runStart = 0;

    for (size_t index = 0; index + 2 < vertexCount; ++index) {
        uint32_t ia = baseVertex + uint32_t(index + 0);
        uint32_t ib = baseVertex + uint32_t(index + 1);
        uint32_t ic = baseVertex + uint32_t(index + 2);

        if (ia >= allPoints.size() || ib >= allPoints.size() || ic >= allPoints.size()) {
            runStart = index + 1;
            continue;
        }

        const StorylandModelPoint& a = allPoints[ia];
        const StorylandModelPoint& b = allPoints[ib];
        const StorylandModelPoint& c = allPoints[ic];

        if (triangleAreaSquared(a, b, c) < 0.0000000001f) {
            // Treat degenerates as primitive restart, not merely skipped faces.
            runStart = index + 1;
            continue;
        }

        StorylandModelTriangle tri;
        size_t localParity = (index >= runStart) ? (index - runStart) : index;
        if ((localParity & 1u) == 0) {
            tri.a = ia;
            tri.b = ib;
            tri.c = ic;
        } else {
            tri.a = ib;
            tri.b = ia;
            tri.c = ic;
        }
        tri.materialIndex = materialIndex;
        triangles.push_back(tri);
    }
}


static void appendTriangleStripPreviewDistanceGuarded(
    uint32_t baseVertex,
    size_t vertexCount,
    uint32_t materialIndex,
    const std::vector<StorylandModelPoint>& allPoints,
    std::vector<StorylandModelTriangle>& triangles,
    float maxEdgeLength
) {
    if (vertexCount < 3) return;

    float maxEdgeLengthSq = maxEdgeLength * maxEdgeLength;
    if (!std::isfinite(maxEdgeLengthSq) || maxEdgeLengthSq <= 0.000001f) {
        appendTriangleStripPreview(baseVertex, vertexCount, materialIndex, allPoints, triangles);
        return;
    }

    size_t runStart = 0;

    for (size_t index = 0; index + 2 < vertexCount; ++index) {
        uint32_t ia = baseVertex + uint32_t(index + 0);
        uint32_t ib = baseVertex + uint32_t(index + 1);
        uint32_t ic = baseVertex + uint32_t(index + 2);

        if (ia >= allPoints.size() || ib >= allPoints.size() || ic >= allPoints.size()) {
            runStart = index + 1;
            continue;
        }

        const StorylandModelPoint& a = allPoints[ia];
        const StorylandModelPoint& b = allPoints[ib];
        const StorylandModelPoint& c = allPoints[ic];

        // MG/vehicle raw streams contain several strip/list chunks back to back.
        // If we connect every row as one mega strip, unrelated wheels/body chunks
        // get bridged by huge triangles.  Treat a long edge as a strip break so
        // winding parity is reset after the break too.
        if (distanceSquared(a, b) > maxEdgeLengthSq ||
            distanceSquared(b, c) > maxEdgeLengthSq ||
            distanceSquared(a, c) > maxEdgeLengthSq) {
            runStart = index + 1;
            continue;
        }

        if (triangleAreaSquared(a, b, c) < 0.0000000001f) {
            runStart = index + 1;
            continue;
        }

        StorylandModelTriangle tri;
        size_t localParity = (index >= runStart) ? (index - runStart) : index;
        if ((localParity & 1u) == 0) {
            tri.a = ia;
            tri.b = ib;
            tri.c = ic;
        } else {
            tri.a = ib;
            tri.b = ia;
            tri.c = ic;
        }
        tri.materialIndex = materialIndex;
        triangles.push_back(tri);
    }
}



static size_t alignModelOffset4(size_t offset) {
    return (offset + 3u) & ~size_t(3u);
}


static uint32_t packedPedSkinBoneIndexFromWord(uint32_t packedWord) {
    // Same decode model as BLeeds:
    // byte 0 is the compact RslTAnim palette token: bone_index * 4.
    // bytes 1..3 are the high bytes of the IEEE float weight.
    uint32_t boneToken = packedWord & 0x000000FFu;
    if ((boneToken % 4u) != 0u) {
        return 0xFFFFFFFFu;
    }
    return boneToken / 4u;
}

static float packedPedSkinWeightFromWord(uint32_t packedWord) {
    // BLeeds-compatible decode:
    // clear only byte 0, not the whole low 16 bits.
    // The previous Storyland decode used 0xFFFF0000 and threw away byte 1
    // of the float, which damaged many weights and made the skinning look
    // like bad vertex groups.
    uint32_t weightBits = packedWord & 0xFFFFFF00u;
    float weight = 0.0f;
    std::memcpy(&weight, &weightBits, sizeof(float));

    if (!std::isfinite(weight) || weight < 0.0f || weight > 4.0f) {
        return 0.0f;
    }

    return weight;
}

static size_t findNextExactLeedsSplitMarkerOffset(
    const std::vector<uint8_t>& data,
    size_t searchStart,
    size_t searchEnd
) {
    searchEnd = std::min(searchEnd, data.size());
    if (searchStart >= searchEnd) {
        return SIZE_MAX;
    }

    for (size_t probe = alignModelOffset4(searchStart); probe + 0x34 <= searchEnd; probe += 4u) {
        if (modelReadU32(data, probe) != 0x6C018000u) {
            continue;
        }

        uint8_t count = data[probe + 0x32u];
        if (count >= 3u && count <= 128u) {
            return probe;
        }
    }

    return SIZE_MAX;
}

static bool decodeLeedsPedSkinWeights(
    const std::vector<uint8_t>& data,
    size_t searchStart,
    size_t searchEnd,
    uint8_t vertexCount,
    std::vector<StorylandModelSkinWeights>& blockSkinWeights
) {
    blockSkinWeights.clear();

    if (vertexCount == 0) return false;
    if (searchStart >= data.size()) return false;
    searchEnd = std::min(searchEnd, data.size());
    if (searchEnd <= searchStart) return false;

    size_t skinHeaderOffset = SIZE_MAX;
    for (size_t probe = searchStart; probe + 4u <= searchEnd; probe += 4u) {
        uint8_t immediateLo = data[probe + 0u];
        uint8_t immediateHi = data[probe + 1u];
        uint8_t count = data[probe + 2u];
        uint8_t command = data[probe + 3u];

        if (command != 0x6Cu) continue;
        if (count != vertexCount) continue;
        if ((immediateHi & 0x80u) == 0) continue;
        if (immediateLo == 0x00u) continue;

        size_t payloadOffset = probe + 4u;
        size_t payloadSize = size_t(vertexCount) * 16u;
        if (payloadOffset + payloadSize > searchEnd) continue;

        skinHeaderOffset = probe;
        break;
    }

    if (skinHeaderOffset == SIZE_MAX) {
        for (size_t probe = searchStart; probe + 4u <= searchEnd; probe += 4u) {
            uint8_t count = data[probe + 2u];
            uint8_t command = data[probe + 3u];

            if (command != 0x6Cu) continue;
            if (count != vertexCount) continue;

            size_t payloadOffset = probe + 4u;
            size_t payloadSize = size_t(vertexCount) * 16u;
            if (payloadOffset + payloadSize > searchEnd) continue;

            uint32_t saneWordCount = 0;
            for (uint32_t testVertex = 0; testVertex < std::min<uint32_t>(vertexCount, 8u); ++testVertex) {
                for (uint32_t influenceIndex = 0; influenceIndex < 4u; ++influenceIndex) {
                    size_t wordOffset = payloadOffset + size_t(testVertex) * 16u + size_t(influenceIndex) * 4u;
                    uint32_t packedWord = modelReadU32(data, wordOffset);
                    uint32_t boneIndex = packedPedSkinBoneIndexFromWord(packedWord);
                    float weight = packedPedSkinWeightFromWord(packedWord);
                    if (boneIndex != 0xFFFFFFFFu && boneIndex <= 255u && weight > 0.00001f && weight <= 4.0f) {
                        saneWordCount++;
                    }
                }
            }

            if (saneWordCount > 0u) {
                skinHeaderOffset = probe;
                break;
            }
        }
    }

    if (skinHeaderOffset == SIZE_MAX) {
        blockSkinWeights.assign(vertexCount, StorylandModelSkinWeights{});
        return false;
    }

    blockSkinWeights.reserve(vertexCount);

    size_t payloadOffset = skinHeaderOffset + 4u;
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        StorylandModelSkinWeights weights;
        float totalWeight = 0.0f;

        for (uint32_t influenceIndex = 0; influenceIndex < 4; ++influenceIndex) {
            size_t wordOffset = payloadOffset + size_t(vertexIndex) * 16u + size_t(influenceIndex) * 4u;
            uint32_t packedWord = modelReadU32(data, wordOffset);
            uint32_t rawMatrixIndex = packedWord & 0x000000FFu;
            uint32_t boneIndex = packedPedSkinBoneIndexFromWord(packedWord);
            float weight = packedPedSkinWeightFromWord(packedWord);

            if (weight <= 0.00001f) {
                continue;
            }

            if (boneIndex == 0xFFFFFFFFu || boneIndex > 255u) {
                continue;
            }

            if (weights.influenceCount < 4u) {
                StorylandModelSkinInfluence& influence = weights.influences[weights.influenceCount++];
                influence.boneIndex = boneIndex;
                influence.rawMatrixIndex = rawMatrixIndex;
                influence.rawPackedWord = packedWord;
                influence.weight = weight;
                totalWeight += weight;
            }
        }

        if (weights.influenceCount > 0u && totalWeight > 0.00001f) {
            for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
                weights.influences[influenceIndex].weight /= totalWeight;
            }
            weights.valid = true;
        }

        blockSkinWeights.push_back(weights);
    }

    return blockSkinWeights.size() == vertexCount;
}


static bool decodeLeedsStripTexcoords(
    const std::vector<uint8_t>& data,
    size_t vertexDataOffset,
    uint8_t vertexCount,
    std::vector<StorylandModelTexcoord>& blockTexcoords
) {
    blockTexcoords.clear();
    if (vertexCount == 0) return false;

    size_t afterVertices = alignModelOffset4(vertexDataOffset + size_t(vertexCount) * 6u);
    if (afterVertices + 0x20 > data.size()) return false;

    size_t uvHeader = SIZE_MAX;

    if (modelReadU32(data, afterVertices + 0x00) == 0x20000000u &&
        modelReadU32(data, afterVertices + 0x08) == 0x30000000u) {
        size_t candidate = afterVertices + 0x1C;
        if (candidate + 4 <= data.size()) {
            uint8_t count = data[candidate + 2];
            uint8_t cmd = data[candidate + 3];
            if (cmd == 0x76 && count > 0 && count <= 0x80) uvHeader = candidate;
        }
    }

    if (uvHeader == SIZE_MAX) {
        size_t searchEnd = std::min(data.size(), afterVertices + 0x100);
        for (size_t probe = afterVertices; probe + 4 <= searchEnd; probe += 4) {
            uint8_t count = data[probe + 2];
            uint8_t cmd = data[probe + 3];
            if (cmd == 0x76 && count > 0 && count <= 0x80) {
                uvHeader = probe;
                break;
            }
        }
    }

    if (uvHeader == SIZE_MAX) return false;
    uint8_t uvCount = data[uvHeader + 2];
    if (uvCount <= 0 || uvCount > 0x80) return false;
    if (uvHeader + 4u + size_t(uvCount) * 2u > data.size()) return false;

    blockTexcoords.reserve(vertexCount);
    for (uint32_t index = 0; index < vertexCount; ++index) {
        uint8_t uRaw = 0;
        uint8_t vRaw = 0;
        if (index < uvCount) {
            size_t uvOffset = uvHeader + 4u + size_t(index) * 2u;
            uRaw = data[uvOffset + 0];
            vRaw = data[uvOffset + 1];
        }
        float u = float(uRaw) / 127.5f;
        float v = float(vRaw) / 127.5f;
        blockTexcoords.push_back({u, v});
    }
    return blockTexcoords.size() == vertexCount;
}


static bool decodeSkinWeightsFromPedPayload(
    const std::vector<uint8_t>& data,
    size_t skinPayloadOffset,
    uint8_t skinCount,
    uint8_t vertexCount,
    std::vector<StorylandModelSkinWeights>& blockSkinWeights
) {
    blockSkinWeights.clear();
    if (vertexCount == 0) {
        return false;
    }

    if (skinCount == 0 || skinCount > 0x80) {
        skinCount = vertexCount;
    }

    if (skinPayloadOffset + size_t(skinCount) * 16u > data.size()) {
        return false;
    }

    blockSkinWeights.reserve(vertexCount);

    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        StorylandModelSkinWeights weights;

        if (vertexIndex < skinCount) {
            float totalWeight = 0.0f;

            for (uint32_t influenceIndex = 0; influenceIndex < 4u; ++influenceIndex) {
                size_t wordOffset = skinPayloadOffset + size_t(vertexIndex) * 16u + size_t(influenceIndex) * 4u;
                uint32_t packedWord = modelReadU32(data, wordOffset);
                uint32_t rawMatrixIndex = packedWord & 0x000000FFu;
                uint32_t boneIndex = packedPedSkinBoneIndexFromWord(packedWord);
                float weight = packedPedSkinWeightFromWord(packedWord);

                if (weight <= 0.00001f) {
                    continue;
                }

                if (boneIndex == 0xFFFFFFFFu || boneIndex > 255u) {
                    continue;
                }

                if (weights.influenceCount < 4u) {
                    StorylandModelSkinInfluence& influence = weights.influences[weights.influenceCount++];
                    influence.boneIndex = boneIndex;
                    influence.rawMatrixIndex = rawMatrixIndex;
                    influence.rawPackedWord = packedWord;
                    influence.weight = weight;
                    totalWeight += weight;
                }
            }

            if (weights.influenceCount > 0u && totalWeight > 0.00001f) {
                for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
                    weights.influences[influenceIndex].weight /= totalWeight;
                }
                weights.valid = true;
            }
        }

        blockSkinWeights.push_back(weights);
    }

    return blockSkinWeights.size() == vertexCount;
}

static bool decodeExactLeedsPedPostVertexPayload(
    const std::vector<uint8_t>& data,
    size_t markerOffset,
    uint8_t vertexCount,
    std::vector<StorylandModelTexcoord>& blockTexcoords,
    std::vector<StorylandModelSkinWeights>& blockSkinWeights
) {
    blockTexcoords.clear();
    blockSkinWeights.clear();

    if (vertexCount == 0) {
        return false;
    }

    size_t cursor = alignModelOffset4(markerOffset + 0x34u + size_t(vertexCount) * 6u);
    if (cursor + 0x20u > data.size()) {
        return false;
    }

    uint32_t stmaskWord = modelReadU32(data, cursor + 0x00u);
    uint32_t strowWord = modelReadU32(data, cursor + 0x08u);
    if (stmaskWord != 0x20000000u || strowWord != 0x30000000u) {
        return false;
    }

    cursor += 0x1Cu;

    if (cursor + 4u > data.size()) {
        return false;
    }

    uint8_t uvCount = data[cursor + 2u];
    uint8_t uvCommand = data[cursor + 3u];
    if (uvCommand != 0x76u) {
        return false;
    }

    if (uvCount == 0 || uvCount > 0x80) {
        uvCount = vertexCount;
    }

    size_t uvPayloadOffset = cursor + 4u;
    if (uvPayloadOffset + size_t(uvCount) * 2u > data.size()) {
        return false;
    }

    blockTexcoords.reserve(vertexCount);
    for (uint32_t index = 0; index < vertexCount; ++index) {
        uint8_t uRaw = 0;
        uint8_t vRaw = 0;
        if (index < uvCount) {
            size_t uvOffset = uvPayloadOffset + size_t(index) * 2u;
            uRaw = data[uvOffset + 0u];
            vRaw = data[uvOffset + 1u];
        }

        blockTexcoords.push_back({float(uRaw) / 127.5f, float(vRaw) / 127.5f});
    }

    cursor = alignModelOffset4(uvPayloadOffset + size_t(uvCount) * 2u);

    if (cursor + 4u > data.size()) {
        return false;
    }

    uint8_t normalCount = data[cursor + 2u];
    uint8_t normalCommand = data[cursor + 3u];
    if (normalCommand != 0x6Au) {
        return false;
    }

    if (normalCount == 0 || normalCount > 0x80) {
        normalCount = vertexCount;
    }

    size_t normalPayloadOffset = cursor + 4u;
    if (normalPayloadOffset + size_t(normalCount) * 3u > data.size()) {
        return false;
    }

    cursor = alignModelOffset4(normalPayloadOffset + size_t(normalCount) * 3u);

    if (cursor + 4u > data.size()) {
        return false;
    }

    uint8_t skinCount = data[cursor + 2u];
    uint8_t skinCommand = data[cursor + 3u];
    if (skinCommand != 0x6Cu) {
        return false;
    }

    size_t skinPayloadOffset = cursor + 4u;
    if (!decodeSkinWeightsFromPedPayload(data, skinPayloadOffset, skinCount, vertexCount, blockSkinWeights)) {
        return false;
    }

    return blockTexcoords.size() == vertexCount && blockSkinWeights.size() == vertexCount;
}


static bool appendExactLeedsSplitMarker(
    const std::vector<uint8_t>& data,
    size_t markerOffset,
    size_t packetSearchEnd,
    uint32_t materialIndex,
    const StorylandPreviewTransform& transform,
    std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelTriangle>& triangles,
    std::vector<StorylandModelTexcoord>& texcoords,
    std::vector<StorylandModelSkinWeights>& skinWeights
) {
    if (markerOffset + 0x34 > data.size()) return false;
    if (modelReadU32(data, markerOffset) != 0x6C018000u) return false;

    uint8_t vertexCount = data[markerOffset + 0x32];
    if (vertexCount < 3 || vertexCount > 128) return false;

    size_t vertexDataOffset = markerOffset + 0x34;
    size_t vertexDataSize = size_t(vertexCount) * 6u;
    if (vertexDataOffset + vertexDataSize > data.size()) return false;

    std::vector<StorylandModelPoint> blockPoints;
    blockPoints.reserve(vertexCount);
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        size_t vertexOffset = vertexDataOffset + size_t(vertexIndex) * 6u;
        int16_t rawX = modelReadI16(data, vertexOffset + 0);
        int16_t rawY = modelReadI16(data, vertexOffset + 2);
        int16_t rawZ = modelReadI16(data, vertexOffset + 4);
        StorylandModelPoint point = decodeLeedsPackedPosition(rawX, rawY, rawZ, transform);
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) return false;
        blockPoints.push_back(point);
    }

    if (!blockHasShape(blockPoints)) return false;

    std::vector<StorylandModelTexcoord> blockTexcoords;
    std::vector<StorylandModelSkinWeights> blockSkinWeights;

    bool exactPedPayloadDecoded = decodeExactLeedsPedPostVertexPayload(
        data,
        markerOffset,
        vertexCount,
        blockTexcoords,
        blockSkinWeights
    );

    if (!exactPedPayloadDecoded) {
        if (!decodeLeedsStripTexcoords(data, vertexDataOffset, vertexCount, blockTexcoords)) {
            blockTexcoords.assign(blockPoints.size(), StorylandModelTexcoord{});
        }
        if (blockTexcoords.size() != blockPoints.size()) {
            blockTexcoords.assign(blockPoints.size(), StorylandModelTexcoord{});
        }

        size_t skinSearchStart = vertexDataOffset + vertexDataSize;

        // Fallback only.  The primary PLR-safe path above parses the exact VIF
        // payload sequentially, because scanning for the next 0x6C018000 marker
        // can be fooled by the same dword appearing inside a large skin/normal
        // payload.  That false next-marker bound was leaving whole PLR mesh
        // chunks unweighted.
        size_t skinSearchEnd = packetSearchEnd;
        size_t localWindowEnd = std::min(data.size(), markerOffset + size_t(0x2000));
        size_t nextMarkerOffset = findNextExactLeedsSplitMarkerOffset(data, markerOffset + 0x34u + vertexDataSize, localWindowEnd);
        if (nextMarkerOffset != SIZE_MAX && nextMarkerOffset > skinSearchStart) {
            skinSearchEnd = nextMarkerOffset;
        } else {
            skinSearchEnd = localWindowEnd;
        }

        if (!decodeLeedsPedSkinWeights(data, skinSearchStart, skinSearchEnd, vertexCount, blockSkinWeights)) {
            blockSkinWeights.assign(blockPoints.size(), StorylandModelSkinWeights{});
        }
    }

    if (blockTexcoords.size() != blockPoints.size()) {
        blockTexcoords.assign(blockPoints.size(), StorylandModelTexcoord{});
    }
    if (blockSkinWeights.size() != blockPoints.size()) {
        blockSkinWeights.assign(blockPoints.size(), StorylandModelSkinWeights{});
    }

    uint32_t baseVertex = uint32_t(points.size());
    points.insert(points.end(), blockPoints.begin(), blockPoints.end());
    texcoords.insert(texcoords.end(), blockTexcoords.begin(), blockTexcoords.end());
    skinWeights.insert(skinWeights.end(), blockSkinWeights.begin(), blockSkinWeights.end());
    appendTriangleStripPreview(baseVertex, blockPoints.size(), materialIndex, points, triangles);
    return true;
}


static bool appendGlobalLeedsStripMarkerFallbackGeometry(
    const std::vector<uint8_t>& data,
    std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelTriangle>& triangles,
    std::vector<StorylandModelTexcoord>& texcoords,
    std::vector<StorylandModelSkinWeights>& skinWeights,
    std::vector<std::string>& materialTextureNames,
    uint32_t& stripCount,
    uint32_t& rejectedMarkerCount,
    uint32_t& partCount,
    bool& usedHeaderTransform
) {
    points.clear();
    triangles.clear();
    texcoords.clear();
    skinWeights.clear();
    materialTextureNames.clear();
    stripCount = 0;
    rejectedMarkerCount = 0;
    partCount = 0;
    usedHeaderTransform = false;

    StorylandPreviewTransform transform;
    StorylandGeometryLayout layout;
    if (findLeedsGeometryLayout(data, layout)) {
        transform = layout.transform;
        usedHeaderTransform = transform.valid;
        materialTextureNames = layout.materialTextureNames;
    }
    if (materialTextureNames.empty()) {
        materialTextureNames.push_back("<global-scan>");
    }

    const size_t maximumPreviewPoints = 250000;
    const size_t maximumPreviewTriangles = 250000;
    size_t lastAcceptedEnd = 0;

    for (size_t markerOffset = 0; markerOffset + 0x34u <= data.size(); markerOffset += 4u) {
        if (modelReadU32(data, markerOffset) != 0x6C018000u) {
            continue;
        }

        uint8_t vertexCount = data[markerOffset + 0x32u];
        if (vertexCount < 3u || vertexCount > 128u) {
            rejectedMarkerCount++;
            continue;
        }

        // Avoid accepting nested false markers inside the payload that was just
        // parsed.  This is deliberately only a post-accept skip; exact payload
        // parsing itself still validates each candidate.
        if (markerOffset < lastAcceptedEnd) {
            continue;
        }

        size_t packetEnd = std::min(data.size(), markerOffset + size_t(0x4000));
        uint32_t materialIndex = 0;
        size_t beforePointCount = points.size();
        size_t beforeTriangleCount = triangles.size();
        size_t beforeSkinWeightCount = skinWeights.size();

        if (appendExactLeedsSplitMarker(data, markerOffset, packetEnd, materialIndex, transform, points, triangles, texcoords, skinWeights)) {
            stripCount++;
            partCount++;
            lastAcceptedEnd = std::max(lastAcceptedEnd, alignModelOffset4(markerOffset + 0x34u + size_t(vertexCount) * 6u));
        } else {
            points.resize(beforePointCount);
            texcoords.resize(beforePointCount);
            skinWeights.resize(beforeSkinWeightCount);
            triangles.resize(beforeTriangleCount);
            rejectedMarkerCount++;
        }

        if (points.size() >= maximumPreviewPoints || triangles.size() >= maximumPreviewTriangles) {
            break;
        }
    }

    return stripCount > 0 && points.size() >= 3 && !triangles.empty();
}

struct StorylandMgCompactStream {
    size_t start = 0;
    size_t count = 0;
};

static bool mgCompactRowLooksLikeVehicleVertex(
    const std::vector<uint8_t>& data,
    size_t row
) {
    if (row + 16u > data.size()) return false;

    int16_t rawX = modelReadI16(data, row + 0u);
    int16_t rawY = modelReadI16(data, row + 2u);
    int16_t rawZ = modelReadI16(data, row + 4u);

    if (rawX < -512 || rawX > 512) return false;
    if (rawY < -512 || rawY > 512) return false;
    if (rawZ < -256 || rawZ > 256) return false;
    if (std::abs(int(rawX)) + std::abs(int(rawY)) + std::abs(int(rawZ)) == 0) return false;

    // The compact vehicle rows in the tested MG resources end with small
    // render/GIF-ish metadata values.  This keeps the scan from running into
    // pointer tables or text.
    uint8_t tailCommand = data[row + 15u];
    return tailCommand == 0x02u || tailCommand == 0x03u || tailCommand == 0x04u || tailCommand == 0x05u || tailCommand == 0x06u;
}

static StorylandModelPoint decodeMgCompactVehiclePoint(
    const std::vector<uint8_t>& data,
    size_t row
) {
    int16_t rawX = modelReadI16(data, row + 0u);
    int16_t rawY = modelReadI16(data, row + 2u);
    int16_t rawZ = modelReadI16(data, row + 4u);

    return StorylandModelPoint{
        float(rawX) / 64.0f,
        float(rawY) / 64.0f,
        float(rawZ) / 64.0f
    };
}

static void appendMgCompactVehicleStream(
    const std::vector<uint8_t>& data,
    const StorylandMgCompactStream& stream,
    uint32_t materialIndex,
    std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelTriangle>& triangles,
    std::vector<StorylandModelTexcoord>& texcoords,
    std::vector<StorylandModelSkinWeights>& skinWeights,
    uint32_t& rejectedMarkerCount
) {
    if (stream.count < 3u) return;

    uint32_t baseVertex = uint32_t(points.size());
    points.reserve(points.size() + stream.count);
    texcoords.reserve(texcoords.size() + stream.count);
    skinWeights.reserve(skinWeights.size() + stream.count);

    for (size_t index = 0; index < stream.count; ++index) {
        size_t row = stream.start + index * 16u;
        if (!mgCompactRowLooksLikeVehicleVertex(data, row)) {
            rejectedMarkerCount++;
            continue;
        }

        StorylandModelPoint point = decodeMgCompactVehiclePoint(data, row);
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            rejectedMarkerCount++;
            continue;
        }

        points.push_back(point);
        texcoords.push_back(StorylandModelTexcoord{});
        skinWeights.push_back(StorylandModelSkinWeights{});
    }

    size_t actualCount = points.size() - baseVertex;
    if (actualCount < 3u) return;

    StorylandModelPoint minPoint = points[baseVertex];
    StorylandModelPoint maxPoint = points[baseVertex];
    for (size_t index = 0; index < actualCount; ++index) {
        const StorylandModelPoint& point = points[size_t(baseVertex) + index];
        minPoint.x = std::min(minPoint.x, point.x); maxPoint.x = std::max(maxPoint.x, point.x);
        minPoint.y = std::min(minPoint.y, point.y); maxPoint.y = std::max(maxPoint.y, point.y);
        minPoint.z = std::min(minPoint.z, point.z); maxPoint.z = std::max(maxPoint.z, point.z);
    }

    float spanX = std::max(0.001f, maxPoint.x - minPoint.x);
    float spanY = std::max(0.001f, maxPoint.y - minPoint.y);
    float spanZ = std::max(0.001f, maxPoint.z - minPoint.z);
    float largestSpan = std::max(spanX, std::max(spanY, spanZ));
    float maxEdgeLength = std::max(0.20f, largestSpan * 0.35f);

    appendTriangleStripPreviewDistanceGuarded(baseVertex, actualCount, materialIndex, points, triangles, maxEdgeLength);
}

static bool streamOverlapsRange(const StorylandMgCompactStream& stream, size_t start, size_t end) {
    size_t streamStart = stream.start;
    size_t streamEnd = stream.start + stream.count * 16u;
    return streamStart < end && streamEnd > start;
}

static bool streamLooksLikeDuplicateBodyLod(
    const StorylandMgCompactStream& stream,
    size_t primaryStart,
    size_t primaryCount
) {
    if (stream.count < 512u || primaryCount < 512u) return false;
    if (stream.start <= primaryStart) return false;

    // MG vehicle files often carry repeated body/LOD/damage streams before
    // smaller atomics/dummies.  Do not draw every duplicate body stream on top
    // of itself; that was another source of black shredded overlays.
    return stream.count >= (primaryCount * 3u) / 4u;
}

static bool appendMgVehicleRawPreviewGeometry(
    const std::vector<uint8_t>& data,
    std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelTriangle>& triangles,
    std::vector<StorylandModelTexcoord>& texcoords,
    std::vector<StorylandModelSkinWeights>& skinWeights,
    std::vector<std::string>& materialTextureNames,
    uint32_t& stripCount,
    uint32_t& rejectedMarkerCount,
    uint32_t& partCount,
    bool& usedHeaderTransform
) {
    points.clear();
    triangles.clear();
    texcoords.clear();
    skinWeights.clear();
    materialTextureNames.clear();
    stripCount = 0;
    rejectedMarkerCount = 0;
    partCount = 0;
    usedHeaderTransform = false;

    if (data.size() < 0x40 || data[0] != 'M' || data[1] != 'G' || data[2] != 0 || data[3] != 0) {
        return false;
    }

    // MG vehicle-family resources are not ordinary ldm/AA clumps.  They are
    // compact vehicle streams, and vehicle MDLs are multi-atomic: body/LOD
    // streams first, then smaller wheel/dummy/extra streams near the tail.
    //
    // The old fallback decoded only the first stream and/or connected a whole
    // byte range as one strip.  That produced the "vertical shredded box" view.
    std::vector<StorylandMgCompactStream> streams;

    const size_t firstStreamStart = 0x30u;
    size_t firstStreamEnd = modelReadU32(data, 0x20u);
    if (firstStreamEnd <= firstStreamStart || firstStreamEnd > data.size()) {
        firstStreamEnd = std::min(data.size(), firstStreamStart + size_t(0x4000));
    }

    size_t firstCount = 0;
    for (size_t row = firstStreamStart; row + 16u <= firstStreamEnd; row += 16u) {
        if (!mgCompactRowLooksLikeVehicleVertex(data, row)) break;
        firstCount++;
    }

    if (firstCount >= 3u) {
        streams.push_back({firstStreamStart, firstCount});
    }

    size_t protectedEnd = firstStreamStart + firstCount * 16u;

    // Find smaller additional atomics.  Use base-0 16-byte rows because the
    // compact vehicle vertex rows in the tested MG resources are 16-aligned.
    for (size_t row = 0; row + 16u <= data.size(); row += 16u) {
        if (row < protectedEnd) continue;
        if (!mgCompactRowLooksLikeVehicleVertex(data, row)) continue;

        size_t runStart = row;
        size_t runCount = 0;
        while (row + 16u <= data.size() && mgCompactRowLooksLikeVehicleVertex(data, row)) {
            runCount++;
            row += 16u;
        }

        if (runCount >= 20u && runCount <= 256u) {
            StorylandMgCompactStream stream{runStart, runCount};
            if (!streamOverlapsRange(stream, firstStreamStart, protectedEnd) &&
                !streamLooksLikeDuplicateBodyLod(stream, firstStreamStart, firstCount)) {
                streams.push_back(stream);
            }
        }

        if (row > 0) row -= 16u;
    }

    if (streams.empty()) {
        return false;
    }

    materialTextureNames.push_back("<MG multi-atomic compact stream>");
    points.reserve(20000u);
    texcoords.reserve(20000u);
    skinWeights.reserve(20000u);

    for (size_t streamIndex = 0; streamIndex < streams.size(); ++streamIndex) {
        appendMgCompactVehicleStream(
            data,
            streams[streamIndex],
            uint32_t(streamIndex),
            points,
            triangles,
            texcoords,
            skinWeights,
            rejectedMarkerCount
        );
    }

    stripCount = uint32_t(streams.size());
    partCount = uint32_t(streams.size());

    return points.size() >= 3u && !triangles.empty();
}



static size_t alignModelOffset(size_t offset, size_t alignment) {
    if (alignment <= 1u) return offset;
    return (offset + alignment - 1u) & ~(alignment - 1u);
}

static bool pspNativeGeometryHeaderLooksValid(
    const std::vector<uint8_t>& data,
    uint32_t geometryOffset,
    uint32_t& headerOffset
) {
    if (geometryOffset + 0x68u > data.size()) {
        return false;
    }

    headerOffset = geometryOffset + 0x20u;

    uint32_t size = modelReadU32(data, headerOffset + 0x00u);
    uint32_t flags = modelReadU32(data, headerOffset + 0x04u);
    uint32_t numStrips = modelReadU32(data, headerOffset + 0x08u);
    uint32_t vertexOffset = modelReadU32(data, headerOffset + 0x40u);

    if (size < 0x48u || size > data.size() - headerOffset) return false;
    if (numStrips == 0u || numStrips > 512u) return false;
    if (0x48u + numStrips * 0x30u > size) return false;
    if (vertexOffset < 0x48u + numStrips * 0x30u) return false;
    if (headerOffset + vertexOffset >= data.size()) return false;

    uint32_t uvFormat = flags & 3u;
    uint32_t colorFormat = (flags >> 2u) & 7u;
    uint32_t normalFormat = (flags >> 5u) & 3u;
    uint32_t positionFormat = (flags >> 7u) & 3u;
    uint32_t weightFormat = (flags >> 9u) & 3u;
    uint32_t indexFormat = (flags >> 11u) & 3u;

    if (uvFormat > 1u) return false;
    if (colorFormat != 0u && colorFormat != 5u) return false;
    if (normalFormat > 1u) return false;
    if (positionFormat != 1u && positionFormat != 2u) return false;
    if (weightFormat > 1u) return false;
    if (indexFormat != 0u) return false;

    float sx = modelReadF32(data, headerOffset + 0x20u);
    float sy = modelReadF32(data, headerOffset + 0x24u);
    float sz = modelReadF32(data, headerOffset + 0x28u);
    float tx = modelReadF32(data, headerOffset + 0x34u);
    float ty = modelReadF32(data, headerOffset + 0x38u);
    float tz = modelReadF32(data, headerOffset + 0x3Cu);

    return previewFiniteReasonable(sx, 1000.0f) &&
           previewFiniteReasonable(sy, 1000.0f) &&
           previewFiniteReasonable(sz, 1000.0f) &&
           previewFiniteReasonable(tx, 1000.0f) &&
           previewFiniteReasonable(ty, 1000.0f) &&
           previewFiniteReasonable(tz, 1000.0f);
}

static bool appendPspNativeGeometry(
    const std::vector<uint8_t>& data,
    std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelTriangle>& triangles,
    std::vector<StorylandModelTexcoord>& texcoords,
    std::vector<StorylandModelSkinWeights>& skinWeights,
    std::vector<std::string>& materialTextureNames,
    uint32_t& stripCount,
    uint32_t& rejectedMarkerCount,
    uint32_t& partCount,
    bool& usedHeaderTransform
) {
    points.clear();
    triangles.clear();
    texcoords.clear();
    skinWeights.clear();
    materialTextureNames.clear();
    stripCount = 0;
    rejectedMarkerCount = 0;
    partCount = 0;
    usedHeaderTransform = false;

    bool decodedAnyGeometry = false;

    for (size_t atomicOffset = 0; atomicOffset + 0x38u <= data.size(); atomicOffset += 4u) {
        uint32_t sectionId = modelReadU32(data, atomicOffset);
        if (sectionId != 0x01050001u && sectionId != 0x0004AA01u && sectionId != 0x0000AA01u && ((sectionId & 0xFFFF00FFu) != 0x00040001u)) {
            continue;
        }

        uint32_t geometryOffset = modelReadU32(data, atomicOffset + 0x14u);
        if (!modelPointerLooksValid(data, geometryOffset)) {
            continue;
        }

        uint32_t headerOffset = 0;
        if (!pspNativeGeometryHeaderLooksValid(data, geometryOffset, headerOffset)) {
            continue;
        }

        uint32_t materialListPointer = modelReadU32(data, geometryOffset + 0x0Cu);
        uint32_t materialCount = modelReadU32(data, geometryOffset + 0x10u);
        std::vector<std::string> localMaterials;
        if (materialCount > 0u && materialCount <= 512u && modelPointerLooksValid(data, materialListPointer) && materialListPointer + materialCount * 4u <= data.size()) {
            for (uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
                uint32_t materialPointer = modelReadU32(data, materialListPointer + materialIndex * 4u);
                std::string textureName;
                if (modelPointerLooksValid(data, materialPointer) && materialPointer + 0x10u <= data.size()) {
                    uint32_t textureNamePointer = modelReadU32(data, materialPointer + 0x00u);
                    textureName = readModelCStringAt(data, textureNamePointer);
                }
                localMaterials.push_back(modelLowerAscii(textureName));
            }
        }
        if (localMaterials.empty()) {
            localMaterials.push_back("<LCS/PSP native>");
        }

        if (materialTextureNames.empty()) {
            materialTextureNames = localMaterials;
        } else if (materialTextureNames.size() < localMaterials.size()) {
            materialTextureNames.resize(localMaterials.size());
            for (size_t index = 0; index < localMaterials.size(); ++index) {
                if (materialTextureNames[index].empty()) materialTextureNames[index] = localMaterials[index];
            }
        }

        uint32_t size = modelReadU32(data, headerOffset + 0x00u);
        uint32_t flags = modelReadU32(data, headerOffset + 0x04u);
        uint32_t numStrips = modelReadU32(data, headerOffset + 0x08u);
        uint32_t vertexBaseOffset = modelReadU32(data, headerOffset + 0x40u);

        uint32_t uvFormat = flags & 3u;
        uint32_t colorFormat = (flags >> 2u) & 7u;
        uint32_t normalFormat = (flags >> 5u) & 3u;
        uint32_t positionFormat = (flags >> 7u) & 3u;
        uint32_t weightFormat = (flags >> 9u) & 3u;
        uint32_t numberOfWeights = ((flags >> 14u) & 7u) + 1u;

        float scaleX = modelReadF32(data, headerOffset + 0x20u);
        float scaleY = modelReadF32(data, headerOffset + 0x24u);
        float scaleZ = modelReadF32(data, headerOffset + 0x28u);
        float posX = modelReadF32(data, headerOffset + 0x34u);
        float posY = modelReadF32(data, headerOffset + 0x38u);
        float posZ = modelReadF32(data, headerOffset + 0x3Cu);

        for (uint32_t stripIndex = 0; stripIndex < numStrips; ++stripIndex) {
            size_t stripHeader = size_t(headerOffset) + 0x48u + size_t(stripIndex) * 0x30u;
            if (stripHeader + 0x30u > data.size()) {
                rejectedMarkerCount++;
                continue;
            }

            uint32_t stripVertexOffset = modelReadU32(data, stripHeader + 0x00u);
            uint32_t triangleCount = modelReadU16(data, stripHeader + 0x04u);
            uint32_t materialIndex = modelReadU16(data, stripHeader + 0x06u);
            float uvScaleX = modelReadF32(data, stripHeader + 0x0Cu);
            float uvScaleY = modelReadF32(data, stripHeader + 0x10u);

            uint8_t boneMap[8] = {};
            for (uint32_t boneMapIndex = 0; boneMapIndex < 8u; ++boneMapIndex) {
                if (stripHeader + 0x28u + boneMapIndex < data.size()) {
                    boneMap[boneMapIndex] = data[stripHeader + 0x28u + boneMapIndex];
                }
            }

            uint32_t vertexCount = triangleCount + 2u;
            if (vertexCount < 3u || vertexCount > 65535u) {
                rejectedMarkerCount++;
                continue;
            }

            size_t streamOffset = size_t(headerOffset) + size_t(vertexBaseOffset) + size_t(stripVertexOffset);
            if (streamOffset >= data.size()) {
                rejectedMarkerCount++;
                continue;
            }

            uint32_t baseVertex = uint32_t(points.size());
            size_t cursor = 0;
            bool stripOk = true;

            for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                uint8_t rawWeights[8] = {};

                if (weightFormat == 1u) {
                    if (streamOffset + cursor + numberOfWeights > data.size()) {
                        stripOk = false;
                        break;
                    }
                    for (uint32_t weightIndex = 0; weightIndex < numberOfWeights && weightIndex < 8u; ++weightIndex) {
                        rawWeights[weightIndex] = data[streamOffset + cursor + weightIndex];
                    }
                    cursor += numberOfWeights;
                }

                float u = 0.0f;
                float v = 0.0f;
                if (uvFormat == 1u) {
                    if (streamOffset + cursor + 2u > data.size()) {
                        stripOk = false;
                        break;
                    }
                    float safeUvScaleX = std::isfinite(uvScaleX) && std::fabs(uvScaleX) > 0.000001f ? uvScaleX : 1.0f;
                    float safeUvScaleY = std::isfinite(uvScaleY) && std::fabs(uvScaleY) > 0.000001f ? uvScaleY : 1.0f;
                    u = float(data[streamOffset + cursor + 0u]) / 128.0f * safeUvScaleX;
                    v = float(data[streamOffset + cursor + 1u]) / 128.0f * safeUvScaleY;
                    cursor += 2u;
                }

                if (colorFormat == 5u) {
                    cursor = alignModelOffset(cursor, 2u);
                    if (streamOffset + cursor + 2u > data.size()) {
                        stripOk = false;
                        break;
                    }
                    cursor += 2u;
                }

                if (normalFormat == 1u) {
                    if (streamOffset + cursor + 3u > data.size()) {
                        stripOk = false;
                        break;
                    }
                    cursor += 3u;
                }

                StorylandModelPoint point;
                if (positionFormat == 1u) {
                    if (streamOffset + cursor + 3u > data.size()) {
                        stripOk = false;
                        break;
                    }
                    int8_t rawX = static_cast<int8_t>(data[streamOffset + cursor + 0u]);
                    int8_t rawY = static_cast<int8_t>(data[streamOffset + cursor + 1u]);
                    int8_t rawZ = static_cast<int8_t>(data[streamOffset + cursor + 2u]);
                    point.x = float(rawX) / 128.0f * scaleX + posX;
                    point.y = float(rawY) / 128.0f * scaleY + posY;
                    point.z = float(rawZ) / 128.0f * scaleZ + posZ;
                    cursor += 3u;
                } else if (positionFormat == 2u) {
                    cursor = alignModelOffset(cursor, 2u);
                    if (streamOffset + cursor + 6u > data.size()) {
                        stripOk = false;
                        break;
                    }
                    int16_t rawX = modelReadI16(data, streamOffset + cursor + 0u);
                    int16_t rawY = modelReadI16(data, streamOffset + cursor + 2u);
                    int16_t rawZ = modelReadI16(data, streamOffset + cursor + 4u);
                    point.x = float(rawX) / 32768.0f * scaleX + posX;
                    point.y = float(rawY) / 32768.0f * scaleY + posY;
                    point.z = float(rawZ) / 32768.0f * scaleZ + posZ;
                    cursor += 6u;
                } else {
                    stripOk = false;
                    break;
                }

                if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
                    stripOk = false;
                    break;
                }

                points.push_back(point);
                texcoords.push_back({u, v});

                StorylandModelSkinWeights weights;
                if (weightFormat == 1u) {
                    std::vector<uint32_t> order;
                    for (uint32_t weightIndex = 0; weightIndex < numberOfWeights && weightIndex < 8u; ++weightIndex) {
                        order.push_back(weightIndex);
                    }
                    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
                        return rawWeights[a] > rawWeights[b];
                    });

                    float totalWeight = 0.0f;
                    for (uint32_t orderIndex = 0; orderIndex < order.size() && weights.influenceCount < 4u; ++orderIndex) {
                        uint32_t weightIndex = order[orderIndex];
                        if (rawWeights[weightIndex] == 0u) continue;

                        StorylandModelSkinInfluence& influence = weights.influences[weights.influenceCount++];
                        influence.boneIndex = boneMap[weightIndex];
                        influence.rawMatrixIndex = boneMap[weightIndex];
                        influence.rawPackedWord = rawWeights[weightIndex];
                        influence.weight = float(rawWeights[weightIndex]) / 128.0f;
                        totalWeight += influence.weight;
                    }

                    if (weights.influenceCount > 0u && totalWeight > 0.00001f) {
                        for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
                            weights.influences[influenceIndex].weight /= totalWeight;
                        }
                        weights.valid = true;
                    }
                }
                skinWeights.push_back(weights);
            }

            if (!stripOk || points.size() < size_t(baseVertex) + 3u) {
                points.resize(baseVertex);
                texcoords.resize(baseVertex);
                skinWeights.resize(baseVertex);
                rejectedMarkerCount++;
                continue;
            }

            appendTriangleStripPreview(baseVertex, points.size() - baseVertex, materialIndex, points, triangles);
            stripCount++;
            decodedAnyGeometry = true;
        }

        partCount += numStrips;
        usedHeaderTransform = true;
    }

    return decodedAnyGeometry && points.size() >= 3u && !triangles.empty();
}

static bool appendExactLeedsStripGeometry(
    const std::vector<uint8_t>& data,
    std::vector<StorylandModelPoint>& points,
    std::vector<StorylandModelTriangle>& triangles,
    std::vector<StorylandModelTexcoord>& texcoords,
    std::vector<StorylandModelSkinWeights>& skinWeights,
    std::vector<std::string>& materialTextureNames,
    uint32_t& stripCount,
    uint32_t& rejectedMarkerCount,
    uint32_t& partCount,
    bool& usedHeaderTransform
) {
    points.clear();
    triangles.clear();
    texcoords.clear();
    skinWeights.clear();
    materialTextureNames.clear();
    stripCount = 0;
    rejectedMarkerCount = 0;
    partCount = 0;
    usedHeaderTransform = false;

    if (data.size() >= 4 && data[0] == 'M' && data[1] == 'G' && data[2] == 0 && data[3] == 0) {
        return appendMgVehicleRawPreviewGeometry(data, points, triangles, texcoords, skinWeights, materialTextureNames, stripCount, rejectedMarkerCount, partCount, usedHeaderTransform);
    }

    if (appendPspNativeGeometry(data, points, triangles, texcoords, skinWeights, materialTextureNames, stripCount, rejectedMarkerCount, partCount, usedHeaderTransform)) {
        return true;
    }

    StorylandGeometryLayout layout;
    if (!findLeedsGeometryLayout(data, layout)) {
        return appendGlobalLeedsStripMarkerFallbackGeometry(data, points, triangles, texcoords, skinWeights, materialTextureNames, stripCount, rejectedMarkerCount, partCount, usedHeaderTransform);
    }

    usedHeaderTransform = layout.transform.valid;
    materialTextureNames = layout.materialTextureNames;
    partCount = uint32_t(layout.parts.size());

    const size_t maximumPreviewPoints = 200000;
    const size_t maximumPreviewTriangles = 200000;

    for (size_t partIndex = 0; partIndex < layout.parts.size(); ++partIndex) {
        const StorylandGeometryPartEntry& part = layout.parts[partIndex];
        size_t partStart = size_t(layout.geoStart) + size_t(part.stripOffset);
        if (partStart >= data.size()) {
            rejectedMarkerCount++;
            continue;
        }

        size_t partEnd = data.size();
        if (partIndex + 1 < layout.parts.size()) {
            partEnd = size_t(layout.geoStart) + size_t(layout.parts[partIndex + 1].stripOffset);
        }
        if (partEnd <= partStart || partEnd > data.size()) partEnd = std::min(data.size(), partStart + size_t(0x4000));

        bool partHadPacket = false;
        for (size_t markerOffset = partStart; markerOffset + 0x34 <= partEnd; markerOffset += 4) {
            if (modelReadU32(data, markerOffset) != 0x6C018000u) continue;

            size_t beforePointCount = points.size();
            size_t beforeTriangleCount = triangles.size();
            size_t beforeSkinWeightCount = skinWeights.size();
            if (appendExactLeedsSplitMarker(data, markerOffset, partEnd, part.materialIndex, layout.transform, points, triangles, texcoords, skinWeights)) {
                partHadPacket = true;
                stripCount++;
            } else {
                points.resize(beforePointCount);
                texcoords.resize(beforePointCount);
                skinWeights.resize(beforeSkinWeightCount);
                triangles.resize(beforeTriangleCount);
                rejectedMarkerCount++;
            }

            if (points.size() >= maximumPreviewPoints || triangles.size() >= maximumPreviewTriangles) break;
        }

        if (!partHadPacket) rejectedMarkerCount++;
        if (points.size() >= maximumPreviewPoints || triangles.size() >= maximumPreviewTriangles) break;
    }

    if (stripCount > 0 && points.size() >= 3 && !triangles.empty()) {
        // Some simple/weapon/non-standard Leeds MDLs have a usable header but an
        // incomplete material part table.  If a global exact-marker pass finds
        // substantially more vertices, prefer it.  This fixes files like simple
        // weapon models where only the first material-bound chunk was visible.
        std::vector<StorylandModelPoint> globalPoints;
        std::vector<StorylandModelTriangle> globalTriangles;
        std::vector<StorylandModelTexcoord> globalTexcoords;
        std::vector<StorylandModelSkinWeights> globalSkinWeights;
        std::vector<std::string> globalMaterials;
        uint32_t globalStripCount = 0;
        uint32_t globalRejectedCount = 0;
        uint32_t globalPartCount = 0;
        bool globalUsedTransform = false;
        if (appendGlobalLeedsStripMarkerFallbackGeometry(data, globalPoints, globalTriangles, globalTexcoords, globalSkinWeights, globalMaterials, globalStripCount, globalRejectedCount, globalPartCount, globalUsedTransform)) {
            if (globalPoints.size() > points.size() + std::max<size_t>(16u, points.size() / 10u)) {
                points.swap(globalPoints);
                triangles.swap(globalTriangles);
                texcoords.swap(globalTexcoords);
                skinWeights.swap(globalSkinWeights);
                materialTextureNames.swap(globalMaterials);
                stripCount = globalStripCount;
                rejectedMarkerCount += globalRejectedCount;
                partCount = globalPartCount;
                usedHeaderTransform = globalUsedTransform;
            }
        }
        return true;
    }

    return false;
}

void StorylandModelFile::collectPreviewPoints() {
    points.clear();
    triangles.clear();
    texcoords.clear();
    skinWeights.clear();
    materialTextureNames.clear();

    uint32_t stripCount = 0;
    uint32_t rejectedMarkerCount = 0;
    uint32_t partCount = 0;
    bool usedHeaderTransform = false;
    if (appendExactLeedsStripGeometry(data, points, triangles, texcoords, skinWeights, materialTextureNames, stripCount, rejectedMarkerCount, partCount, usedHeaderTransform)) {
        std::ostringstream line;
        line << "Preview: " << points.size() << " vertices, " << triangles.size()
             << " triangles, strips=" << stripCount << ", parts=" << partCount;
        line << (usedHeaderTransform ? ", header transform=yes" : ", header transform=no");
        if (data.size() >= 4 && data[0] == 'M' && data[1] == 'G' && data[2] == 0 && data[3] == 0) {
            line << ", MG compact path";
        } else if (partCount > 0 && materialTextureNames.size() == 1 && materialTextureNames[0] == "<global-scan>") {
            line << ", global scan";
        }
        if (rejectedMarkerCount > 0) line << ", rejected=" << rejectedMarkerCount;
        outputLines.push_back({line.str()});

        if (!materialTextureNames.empty()) {
            std::ostringstream mats;
            mats << "Materials:";
            for (size_t index = 0; index < materialTextureNames.size(); ++index) {
                mats << " " << index << ":" << (materialTextureNames[index].empty() ? "-" : materialTextureNames[index]);
            }
            outputLines.push_back({mats.str()});
        }

        if (skinWeights.size() == points.size()) {
            uint32_t validSkinWeightCount = 0;
            uint32_t influenceTotal = 0;
            for (const auto& weights : skinWeights) {
                if (!weights.valid) continue;
                validSkinWeightCount++;
                influenceTotal += weights.influenceCount;
            }

            if (kind == StorylandModelKind::PedModel || kind == StorylandModelKind::CutsceneModel || validSkinWeightCount > 0) {
                std::ostringstream skinLine;
                skinLine << "Skin: " << validSkinWeightCount << "/" << skinWeights.size() << " weighted vertices";
                if (validSkinWeightCount > 0) skinLine << ", influences=" << influenceTotal;
                outputLines.push_back({skinLine.str()});
            } else {
                outputLines.push_back({"Skin: skipped for rigid model type."});
            }
        }
        return;
    }

    outputLines.push_back({"Preview: no Leeds strip markers found; using float point runs."});

    struct RunCandidate { size_t offset = 0; size_t count = 0; };
    std::vector<RunCandidate> runs;

    size_t runStart = 0;
    size_t runCount = 0;
    for (size_t offset = 0; offset + 12 <= data.size(); offset += 4) {
        float x = modelReadF32(data, offset + 0);
        float y = modelReadF32(data, offset + 4);
        float z = modelReadF32(data, offset + 8);
        bool sane = sanePreviewFloat(x) && sanePreviewFloat(y) && sanePreviewFloat(z);
        bool notAllZero = std::fabs(x) + std::fabs(y) + std::fabs(z) > 0.0001f;
        if (sane && notAllZero) {
            if (runCount == 0) runStart = offset;
            runCount++;
        } else {
            if (runCount >= 12) runs.push_back({runStart, runCount});
            runCount = 0;
        }
    }
    if (runCount >= 12) runs.push_back({runStart, runCount});

    std::sort(runs.begin(), runs.end(), [](const RunCandidate& a, const RunCandidate& b) { return a.count > b.count; });
    if (runs.empty()) return;

    size_t selectedOffset = runs.front().offset;
    size_t selectedCount = std::min<size_t>(runs.front().count, 4096);
    for (size_t i = 0; i < selectedCount; ++i) {
        size_t offset = selectedOffset + i * 4;
        if (offset + 12 > data.size()) break;
        float x = modelReadF32(data, offset + 0);
        float y = modelReadF32(data, offset + 4);
        float z = modelReadF32(data, offset + 8);
        if (sanePreviewFloat(x) && sanePreviewFloat(y) && sanePreviewFloat(z)) {
            points.push_back({x, y, z});
            texcoords.push_back({0.0f, 0.0f});
        }
    }
}


void StorylandModelFile::collectArmatureBones() {
    bones.clear();

    if (kind != StorylandModelKind::PedModel && kind != StorylandModelKind::CutsceneModel) {
        return;
    }


    struct NodeCandidate {
        uint32_t offset = 0;
        std::string kind;
        uint32_t parentPtr = 0;
        uint32_t childPtr = 0;
        uint32_t nextPtr = 0;
        uint32_t rootPtr = 0;
        uint32_t nodeId = 0xFFFFFFFFu;
        bool hasLocalPosition = false;
        bool hasLocalRotation = false;
        bool hasWorldPosition = false;
        bool hasWorldRotation = false;
        StorylandModelPoint localPosition;
        StorylandModelPoint worldPosition;
        StorylandModelMatrix localMatrix;
        StorylandModelMatrix worldMatrix;
        float localRotationX = 0.0f;
        float localRotationY = 0.0f;
        float localRotationZ = 0.0f;
        float localRotationW = 1.0f;
        float worldRotationX = 0.0f;
        float worldRotationY = 0.0f;
        float worldRotationZ = 0.0f;
        float worldRotationW = 1.0f;
    };

    struct HierarchyInfo {
        bool valid = false;
        uint32_t offset = 0;
        uint32_t count = 0;
        uint32_t entriesPtr = 0;
        uint32_t anchorFrame = 0;
    };

    struct HierarchyEntry {
        uint32_t packed = 0;
        uint32_t boneId = 0;
        uint32_t nodeIndex = 0;
        uint32_t boneType = 0;
        uint32_t frameOffset = 0;
    };

    std::vector<NodeCandidate> nodes;
    std::map<uint32_t, uint32_t> offsetToNode;

    for (size_t offset = 0; offset + 0xA0 <= data.size(); offset += 4) {
        uint32_t sectionId = modelReadU32(data, offset);
        std::string kindName = classifySectionId(sectionId);
        if (kindName != "RslNode1" && kindName != "RslNode2") continue;

        StorylandModelPoint localPosition = readModelMatrixPosition(data, uint32_t(offset + 0x10));
        StorylandModelMatrix localMatrix = readModelMatrix(data, uint32_t(offset + 0x10));
        StorylandModelMatrix worldMatrix = readModelMatrix(data, uint32_t(offset + 0x50));
        StorylandModelPoint worldPosition = matrixModelPosition(worldMatrix);

        float localRotationX = 0.0f;
        float localRotationY = 0.0f;
        float localRotationZ = 0.0f;
        float localRotationW = 1.0f;
        bool hasLocalRotation = matrixModelRotationToQuat(localMatrix, localRotationX, localRotationY, localRotationZ, localRotationW);

        float worldRotationX = 0.0f;
        float worldRotationY = 0.0f;
        float worldRotationZ = 0.0f;
        float worldRotationW = 1.0f;
        bool hasWorldRotation = matrixModelRotationToQuat(worldMatrix, worldRotationX, worldRotationY, worldRotationZ, worldRotationW);

        bool hasLocalPosition = modelPointLooksReasonable(localPosition, 10000.0f);
        bool hasWorldPosition = modelPointLooksReasonable(worldPosition, 10000.0f);
        if (!hasLocalPosition && !hasWorldPosition) continue;

        uint32_t candidateRootPtr = modelReadU32(data, offset + 0x98);
        uint32_t candidateParentPtr = modelReadU32(data, offset + 0x04);
        bool pspOrLcsFrameTag = sectionId == 0x0C000000u || ((sectionId & 0xFF0000FFu) == 0x01000000u) || ((sectionId & 0xFF0000FFu) == 0x03000000u);
        if (pspOrLcsFrameTag) {
            bool rootPtrLooksUseful = candidateRootPtr == 0 || modelPointerLooksValid(data, candidateRootPtr);
            bool parentPtrLooksUseful = candidateParentPtr == 0 || modelPointerLooksValid(data, candidateParentPtr);
            if (!rootPtrLooksUseful || !parentPtrLooksUseful) continue;
        }

        NodeCandidate node;
        node.offset = uint32_t(offset);
        node.kind = kindName;
        node.parentPtr = modelReadU32(data, offset + 0x04);
        node.childPtr = modelReadU32(data, offset + 0x90);
        node.nextPtr = modelReadU32(data, offset + 0x94);
        node.rootPtr = modelReadU32(data, offset + 0x98);
        node.nodeId = modelReadU32(data, offset + 0x9C);
        node.localPosition = localPosition;
        node.worldPosition = worldPosition;
        node.localMatrix = localMatrix;
        node.worldMatrix = worldMatrix;
        node.localRotationX = localRotationX;
        node.localRotationY = localRotationY;
        node.localRotationZ = localRotationZ;
        node.localRotationW = localRotationW;
        node.worldRotationX = worldRotationX;
        node.worldRotationY = worldRotationY;
        node.worldRotationZ = worldRotationZ;
        node.worldRotationW = worldRotationW;
        node.hasLocalPosition = hasLocalPosition;
        node.hasLocalRotation = hasLocalRotation;
        node.hasWorldPosition = hasWorldPosition;
        node.hasWorldRotation = hasWorldRotation;

        offsetToNode[node.offset] = uint32_t(nodes.size());
        nodes.push_back(node);
    }

    if (nodes.empty()) return;

    auto nodeExists = [&](uint32_t offset) -> bool {
        return offsetToNode.find(offset) != offsetToNode.end();
    };

    std::map<uint32_t, uint32_t> directBoneIdToNodeOffset;
    for (const NodeCandidate& node : nodes) {
        if (node.nodeId == 0xFFFFFFFFu) continue;
        if (node.nodeId > 10000u) continue;
        // LCS PS2 PED/Cutscene samples from the regression corpus store the real
        // HAnim/direct bone id in RslNode +0x9C.  Frame allocation is randomized
        // per file, so using allocation order or child/sibling traversal maps
        // bones onto the wrong frames.
        directBoneIdToNodeOffset[node.nodeId] = node.offset;
    }

    HierarchyInfo hierarchy;
    for (size_t atomicOffset = 0; atomicOffset + 0x34 <= data.size(); atomicOffset += 4) {
        uint32_t sectionId = modelReadU32(data, atomicOffset);
        std::string kindName = classifySectionId(sectionId);
        if (kindName != "Atomic") continue;

        uint32_t candidate = modelReadU32(data, atomicOffset + 0x2C);
        if (candidate + 0x38 > data.size()) continue;
        uint32_t tag = modelReadU32(data, candidate + 0x00);
        uint32_t count = modelReadU32(data, candidate + 0x04);
        uint32_t entriesPtr = modelReadU32(data, candidate + 0x30);
        uint32_t anchorFrame = modelReadU32(data, candidate + 0x34);
        if (tag != 0x00003000u) continue;
        if (count == 0 || count > 80) continue;
        if (entriesPtr == 0 || entriesPtr + count * 8u > data.size()) continue;
        if (!nodeExists(anchorFrame)) continue;

        hierarchy.valid = true;
        hierarchy.offset = candidate;
        hierarchy.count = count;
        hierarchy.entriesPtr = entriesPtr;
        hierarchy.anchorFrame = anchorFrame;
        break;
    }

    auto replaceAllAscii = [](std::string& value, const std::string& oldText, const std::string& newText) -> void {
        if (oldText.empty()) return;
        size_t pos = 0;
        while ((pos = value.find(oldText, pos)) != std::string::npos) {
            value.replace(pos, oldText.size(), newText);
            pos += newText.size();
        }
    };

    auto bleedsDirectIdForHierarchyName = [&](const std::string& rawName) -> uint32_t {
        std::string name = rawName;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        for (char& ch : name) {
            if (ch == ' ' || ch == '-' || ch == '.') ch = '_';
        }
        while (name.find("__") != std::string::npos) replaceAllAscii(name, "__", "_");

        if (name.rfind("bip01_", 0) == 0) name = name.substr(6);
        if (name == "scene_root" || name == "pivots" || name == "male_base" || name == "female_base" ||
            name == "male_base01" || name == "female_base01") {
            name = "root";
        }

        replaceAllAscii(name, "right_", "r_");
        replaceAllAscii(name, "left_", "l_");
        replaceAllAscii(name, "upper_arm", "upperarm");
        replaceAllAscii(name, "lower_arm", "forearm");
        replaceAllAscii(name, "lowerarm", "forearm");
        replaceAllAscii(name, "shin", "calf");

        if (name == "root") return 0;
        if (name == "pelvis") return 1;
        if (name == "spine") return 2;
        if (name == "spine1") return 3;
        if (name == "neck") return 4;
        if (name == "head") return 5;
        if (name == "r_clavicle") return 21;
        if (name == "r_upperarm") return 22;
        if (name == "r_forearm") return 23;
        if (name == "r_hand") return 24;
        if (name == "r_finger") return 25;
        if (name == "l_clavicle") return 31;
        if (name == "l_upperarm") return 32;
        if (name == "l_forearm") return 33;
        if (name == "l_hand") return 34;
        if (name == "l_finger") return 35;
        if (name == "l_thigh") return 41;
        if (name == "l_calf") return 42;
        if (name == "l_foot") return 43;
        if (name == "l_toe0" || name == "l_toe") return 54;
        if (name == "r_thigh") return 51;
        if (name == "r_calf") return 52;
        if (name == "r_foot") return 53;
        if (name == "r_toe0" || name == "r_toe") return 55;
        if (name == "jaw") return 8;
        return 0xFFFFFFFFu;
    };

    auto hierarchyNameForDirectBoneId = [](uint32_t boneId) -> std::string {
        switch (boneId) {
        case 0: return "root";
        case 1: return "pelvis";
        case 2: return "spine";
        case 3: return "spine1";
        case 4: return "neck";
        case 5: return "head";
        case 8: return "jaw";
        case 21: return "bip01_r_clavicle";
        case 22: return "r_upperarm";
        case 23: return "r_forearm";
        case 24: return "r_hand";
        case 25: return "r_finger";
        case 31: return "bip01_l_clavicle";
        case 32: return "l_upperarm";
        case 33: return "l_forearm";
        case 34: return "l_hand";
        case 35: return "l_finger";
        case 41: return "l_thigh";
        case 42: return "l_calf";
        case 43: return "l_foot";
        case 51: return "r_thigh";
        case 52: return "r_calf";
        case 53: return "r_foot";
        // LCS PS2 PEDs commonly use direct ids 54/55 for toe nodes instead of
        // the later VCS Storyland display-only 2000/2001 ids.
        case 54: return "l_toe0";
        case 55: return "r_toe0";
        case 2000: return "l_toe0";
        case 2001: return "r_toe0";
        default: break;
        }
        return "";
    };

    auto hierarchyNameForNodeIndex = [&](uint32_t nodeIndex, uint32_t boneId) -> std::string {
        std::string directName = hierarchyNameForDirectBoneId(boneId);
        if (!directName.empty()) return directName;

        // RSL/HAnim hierarchy node_index order for normal VCS/LCS PEDs is not the
        // same as the compact ANIM direct-id order and not the same as the skin
        // palette token meaning in all writer builds.
        //
        // PLR proves this: node_index 3 is the spine1 frame, not r_thigh.
        // The old Storyland table named node 3 as r_thigh, node 11 as spine1,
        // node 18 as l_clavicle, and node 23 as head.  That made the skeleton
        // tree look like head/arms/legs were parented through toes/fingers and
        // then skinning remapped valid weights onto the wrong frames.
        //
        // Use the actual VCS/LCS PED hierarchy traversal order here.
        static const char* names[] = {
            "root",
            "pelvis",
            "spine",
            "spine1",
            "neck",
            "head",
            "jaw",
            "bip01_l_clavicle",
            "l_upperarm",
            "l_forearm",
            "l_hand",
            "l_finger",
            "bip01_r_clavicle",
            "r_upperarm",
            "r_forearm",
            "r_hand",
            "r_finger",
            "l_thigh",
            "l_calf",
            "l_foot",
            "l_toe0",
            "r_thigh",
            "r_calf",
            "r_foot",
            "r_toe0"
        };

        if (nodeIndex < (sizeof(names) / sizeof(names[0]))) return names[nodeIndex];

        std::ostringstream ss;
        ss << "hier_node_" << nodeIndex;
        if (boneId != 0xFFFFFFFFu) ss << "_bone_" << boneId;
        return ss.str();
    };

    std::vector<uint32_t> traversalOrder;
    if (hierarchy.valid) {
        // RslTAnim nodeIndex in Leeds PED MDLs does not reliably mean
        // "depth-first child/sibling traversal index".
        //
        // Cop shows the failure clearly:
        //   child/sibling traversal maps nodeIndex 11 to frame 0x000320,
        //   which then gets named l_finger even though that frame is parented
        //   from spine.  The result is an impossible skeleton and every skin
        //   weight that hits those names deforms around the wrong frame.
        //
        // The hierarchy table is laid out in allocation/nodeIndex order.  Use
        // the frame allocation order starting at anchorFrame, not child-first
        // traversal.  Wrap once for models whose hierarchy block was allocated
        // after later sibling branches.  Prefer RslNode1 frames; only include
        // RslNode2 if the anchor itself is RslNode2 or if there are not enough
        // RslNode1 frames to satisfy the hierarchy count.
        std::vector<uint32_t> allocationOrder;
        allocationOrder.reserve(nodes.size());

        bool anchorIsNode2 = false;
        auto anchorIt = offsetToNode.find(hierarchy.anchorFrame);
        if (anchorIt != offsetToNode.end()) {
            anchorIsNode2 = nodes[anchorIt->second].kind == "RslNode2";
        }

        auto appendAllocationPass = [&](bool includeNode2) {
            allocationOrder.clear();

            std::vector<uint32_t> sortedOffsets;
            sortedOffsets.reserve(nodes.size());
            for (const NodeCandidate& node : nodes) {
                if (node.kind == "RslNode1" || includeNode2 || node.offset == hierarchy.anchorFrame) {
                    sortedOffsets.push_back(node.offset);
                }
            }

            std::sort(sortedOffsets.begin(), sortedOffsets.end());
            if (sortedOffsets.empty()) {
                return;
            }

            size_t startIndex = 0;
            auto foundAnchor = std::find(sortedOffsets.begin(), sortedOffsets.end(), hierarchy.anchorFrame);
            if (foundAnchor != sortedOffsets.end()) {
                startIndex = size_t(foundAnchor - sortedOffsets.begin());
            } else {
                for (size_t index = 0; index < sortedOffsets.size(); ++index) {
                    if (sortedOffsets[index] > hierarchy.anchorFrame) {
                        startIndex = index;
                        break;
                    }
                }
            }

            for (size_t step = 0; step < sortedOffsets.size(); ++step) {
                size_t index = (startIndex + step) % sortedOffsets.size();
                allocationOrder.push_back(sortedOffsets[index]);
                if (allocationOrder.size() >= hierarchy.count) {
                    break;
                }
            }
        };

        appendAllocationPass(anchorIsNode2);
        if (allocationOrder.size() < hierarchy.count) {
            appendAllocationPass(true);
        }

        traversalOrder = allocationOrder;
    }

    std::vector<HierarchyEntry> hierarchyEntries;
    if (hierarchy.valid) {
        hierarchyEntries.reserve(hierarchy.count);
        for (uint32_t index = 0; index < hierarchy.count; ++index) {
            uint32_t packed = modelReadU32(data, size_t(hierarchy.entriesPtr) + size_t(index) * 8u);
            uint32_t packedHigh = (packed >> 24) & 0xFFu;
            uint32_t candidateBoneId = packed & 0xFFu;
            uint32_t candidateNodeIndex = (packed >> 8) & 0xFFu;
            bool vcsStyleEntry = packedHigh == 0xAAu || packedHigh == 0x09u || packedHigh == 0x0Au;
            bool lcsStyleEntry = candidateNodeIndex < hierarchy.count && !hierarchyNameForDirectBoneId(candidateBoneId).empty();

            if (!vcsStyleEntry && !lcsStyleEntry) continue;

            HierarchyEntry entry;
            entry.packed = packed;
            entry.boneId = candidateBoneId;
            entry.nodeIndex = candidateNodeIndex;
            entry.boneType = (packed >> 16) & 0xFFu;

            auto directFrameIt = directBoneIdToNodeOffset.find(entry.boneId);
            if (directFrameIt != directBoneIdToNodeOffset.end()) {
                entry.frameOffset = directFrameIt->second;
            } else if (entry.nodeIndex < traversalOrder.size()) {
                entry.frameOffset = traversalOrder[entry.nodeIndex];
            }
            if (entry.frameOffset != 0 && nodeExists(entry.frameOffset)) {
                hierarchyEntries.push_back(entry);
            }
        }
    }

    std::map<uint32_t, uint32_t> offsetToBone;

    if (!hierarchyEntries.empty()) {
        bones.reserve(hierarchyEntries.size());
        for (uint32_t index = 0; index < hierarchyEntries.size(); ++index) {
            const HierarchyEntry& entry = hierarchyEntries[index];
            const NodeCandidate& node = nodes[offsetToNode[entry.frameOffset]];

            StorylandModelBone bone;
            bone.index = index;
            bone.offset = node.offset;
            bone.sectionKind = node.kind;
            bone.nodeId = entry.nodeIndex;
            bone.name = hierarchyNameForNodeIndex(entry.nodeIndex, entry.boneId);
            bone.boneId = !hierarchyNameForDirectBoneId(entry.boneId).empty()
                ? entry.boneId
                : bleedsDirectIdForHierarchyName(bone.name);
            // Do not fall back to invalid raw hierarchy bytes as ANIM/direct ids.
            // On retail peds it can make r_toe0 inherit 6 and l_toe0 inherit 34,
            // which contaminates animation matching and skin-palette fallback.
            bone.hasLocalPosition = node.hasLocalPosition;
            bone.hasLocalRotation = node.hasLocalRotation;
            bone.hasWorldPosition = node.hasWorldPosition;
            bone.hasWorldRotation = node.hasWorldRotation;
            bone.localRotationX = node.localRotationX;
            bone.localRotationY = node.localRotationY;
            bone.localRotationZ = node.localRotationZ;
            bone.localRotationW = node.localRotationW;
            bone.worldRotationX = node.worldRotationX;
            bone.worldRotationY = node.worldRotationY;
            bone.worldRotationZ = node.worldRotationZ;
            bone.worldRotationW = node.worldRotationW;
            bone.localPosition = node.localPosition;
            bone.worldPosition = node.hasWorldPosition ? node.worldPosition : node.localPosition;
            bone.composedPosition = bone.worldPosition;
            bone.previewPosition = bone.worldPosition;
            bone.previewPositionSource = "RslTAnim hierarchy node, imported_global_0x50 fitted to decoded mesh bounds";
            offsetToBone[bone.offset] = index;
            bones.push_back(bone);
        }

        auto normalizePedBoneLookupName = [&](std::string name) -> std::string {
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            for (char& ch : name) {
                if (ch == ' ' || ch == '-' || ch == '.') ch = '_';
            }
            while (name.find("__") != std::string::npos) replaceAllAscii(name, "__", "_");
            replaceAllAscii(name, "right_", "r_");
            replaceAllAscii(name, "left_", "l_");
            replaceAllAscii(name, "upper_arm", "upperarm");
            replaceAllAscii(name, "lower_arm", "forearm");
            replaceAllAscii(name, "lowerarm", "forearm");
            if (name == "l_clavicle") name = "bip01_l_clavicle";
            if (name == "r_clavicle") name = "bip01_r_clavicle";
            return name;
        };

        auto canonicalPedParentName = [&](const std::string& rawName) -> std::string {
            std::string name = normalizePedBoneLookupName(rawName);
            if (name == "pelvis") return "root";
            if (name == "spine") return "pelvis";
            if (name == "spine1") return "spine";
            if (name == "neck") return "spine1";
            if (name == "head") return "neck";
            if (name == "jaw") return "head";
            if (name == "bip01_l_clavicle") return "spine1";
            if (name == "l_upperarm") return "bip01_l_clavicle";
            if (name == "l_forearm") return "l_upperarm";
            if (name == "l_hand") return "l_forearm";
            if (name == "l_finger") return "l_hand";
            if (name == "bip01_r_clavicle") return "spine1";
            if (name == "r_upperarm") return "bip01_r_clavicle";
            if (name == "r_forearm") return "r_upperarm";
            if (name == "r_hand") return "r_forearm";
            if (name == "r_finger") return "r_hand";
            if (name == "l_thigh") return "pelvis";
            if (name == "l_calf") return "l_thigh";
            if (name == "l_foot") return "l_calf";
            if (name == "l_toe0" || name == "l_toe") return "l_foot";
            if (name == "r_thigh") return "pelvis";
            if (name == "r_calf") return "r_thigh";
            if (name == "r_foot") return "r_calf";
            if (name == "r_toe0" || name == "r_toe") return "r_foot";
            return "";
        };

        std::map<std::string, uint32_t> canonicalNameToBone;
        for (uint32_t index = 0; index < bones.size(); ++index) {
            canonicalNameToBone[normalizePedBoneLookupName(bones[index].name)] = index;
        }

        for (auto& bone : bones) {
            // Retail LCS/PS2 PED frames carry the real parent link in the RslNode
            // frame header.  Do not override that with the generic common-bone
            // table.  Several LCS peds attach thighs under spine and clavicles
            // under neck in the real frame tree; forcing the table's pelvis /
            // spine1 parents makes the viewport armature and ANIM LBS solve look
            // like the legs/arms are connected to the wrong bones.
            const NodeCandidate& node = nodes[offsetToNode[bone.offset]];
            auto parentIt = offsetToBone.find(node.parentPtr);
            if (parentIt != offsetToBone.end() && parentIt->second != bone.index) {
                bone.parentIndex = parentIt->second;
                bone.parentOffset = node.parentPtr;
                continue;
            }

            // Fallback for stripped/custom skeletons whose frame links are absent
            // or whose parent is a non-palette scene/root helper.
            std::string parentName = canonicalPedParentName(bone.name);
            auto canonicalParentIt = canonicalNameToBone.find(parentName);
            if (canonicalParentIt != canonicalNameToBone.end() && canonicalParentIt->second != bone.index) {
                bone.parentIndex = canonicalParentIt->second;
                bone.parentOffset = bones[canonicalParentIt->second].offset;
                continue;
            }
        }
    } else {
        bones.reserve(nodes.size());
        for (uint32_t index = 0; index < nodes.size(); ++index) {
            StorylandModelBone bone;
            bone.index = index;
            bone.offset = nodes[index].offset;
            bone.sectionKind = nodes[index].kind;
            bone.nodeId = nodes[index].nodeId;
            bone.boneId = 0xFFFFFFFFu;
            bone.hasLocalPosition = nodes[index].hasLocalPosition;
            bone.hasLocalRotation = nodes[index].hasLocalRotation;
            bone.hasWorldPosition = nodes[index].hasWorldPosition;
            bone.hasWorldRotation = nodes[index].hasWorldRotation;
            bone.localRotationX = nodes[index].localRotationX;
            bone.localRotationY = nodes[index].localRotationY;
            bone.localRotationZ = nodes[index].localRotationZ;
            bone.localRotationW = nodes[index].localRotationW;
            bone.worldRotationX = nodes[index].worldRotationX;
            bone.worldRotationY = nodes[index].worldRotationY;
            bone.worldRotationZ = nodes[index].worldRotationZ;
            bone.worldRotationW = nodes[index].worldRotationW;
            bone.localPosition = nodes[index].localPosition;
            bone.worldPosition = nodes[index].hasWorldPosition ? nodes[index].worldPosition : nodes[index].localPosition;
            bone.composedPosition = bone.worldPosition;
            bone.previewPosition = bone.worldPosition;
            bone.previewPositionSource = "raw RslNode imported_global_0x50 fallback";
            std::ostringstream name;
            name << "rsl_frame_" << index;
            bone.name = name.str();
            offsetToBone[bone.offset] = index;
            bones.push_back(bone);
        }

        for (uint32_t index = 0; index < nodes.size(); ++index) {
            auto parentIt = offsetToBone.find(nodes[index].parentPtr);
            if (parentIt != offsetToBone.end() && parentIt->second != index) {
                bones[index].parentIndex = parentIt->second;
                bones[index].parentOffset = nodes[index].parentPtr;
            }
        }
    }

    if ((kind != StorylandModelKind::PedModel && kind != StorylandModelKind::CutsceneModel) || points.empty()) {
        for (auto& bone : bones) {
            bone.hasPreviewPosition = bone.hasWorldPosition || bone.hasLocalPosition;
            if (bone.hasPreviewPosition) bone.previewPosition = bone.hasWorldPosition ? bone.worldPosition : bone.localPosition;
        }
        return;
    }

    auto quantile = [](std::vector<float>& values, double q) -> float {
        if (values.empty()) return 0.0f;
        std::sort(values.begin(), values.end());
        double scaled = q * double(values.size() - 1);
        size_t lo = size_t(std::floor(scaled));
        size_t hi = size_t(std::ceil(scaled));
        float t = float(scaled - double(lo));
        return values[lo] * (1.0f - t) + values[hi] * t;
    };

    std::vector<float> xs;
    std::vector<float> ys;
    std::vector<float> zs;
    xs.reserve(points.size());
    ys.reserve(points.size());
    zs.reserve(points.size());
    for (const auto& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) continue;
        xs.push_back(point.x);
        ys.push_back(point.y);
        zs.push_back(point.z);
    }
    if (xs.empty() || ys.empty() || zs.empty()) return;

    StorylandModelPoint meshMin{quantile(xs, 0.005), quantile(ys, 0.005), quantile(zs, 0.005)};
    StorylandModelPoint meshMax{quantile(xs, 0.995), quantile(ys, 0.995), quantile(zs, 0.995)};

    auto pointIsFinite = [](const StorylandModelPoint& p) -> bool {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    };

    auto expandPointBounds = [](StorylandModelPoint& outMin, StorylandModelPoint& outMax, bool& haveBounds, const StorylandModelPoint& p) {
        if (!haveBounds) {
            outMin = p;
            outMax = p;
            haveBounds = true;
        } else {
            outMin.x = std::min(outMin.x, p.x); outMax.x = std::max(outMax.x, p.x);
            outMin.y = std::min(outMin.y, p.y); outMax.y = std::max(outMax.y, p.y);
            outMin.z = std::min(outMin.z, p.z); outMax.z = std::max(outMax.z, p.z);
        }
    };

    StorylandModelPoint worldBoneMin{};
    StorylandModelPoint worldBoneMax{};
    bool haveWorldBoneBounds = false;
    for (const auto& bone : bones) {
        if (!bone.hasWorldPosition) continue;
        const StorylandModelPoint& p = bone.worldPosition;
        if (!pointIsFinite(p)) continue;
        expandPointBounds(worldBoneMin, worldBoneMax, haveWorldBoneBounds, p);
    }

    float worldSpanX = haveWorldBoneBounds ? (worldBoneMax.x - worldBoneMin.x) : 0.0f;
    float worldSpanY = haveWorldBoneBounds ? (worldBoneMax.y - worldBoneMin.y) : 0.0f;
    float worldSpanZ = haveWorldBoneBounds ? (worldBoneMax.z - worldBoneMin.z) : 0.0f;
    bool worldPositionsHaveUsefulSpan =
        haveWorldBoneBounds &&
        std::isfinite(worldSpanX) && std::isfinite(worldSpanY) && std::isfinite(worldSpanZ) &&
        std::max(worldSpanX, std::max(worldSpanY, worldSpanZ)) > 0.0005f;

    bool useZeroWorldFallbackOverlay = !worldPositionsHaveUsefulSpan;

    float meshSpanX = std::max(0.0001f, meshMax.x - meshMin.x);
    float meshSpanY = std::max(0.0001f, meshMax.y - meshMin.y);
    float meshSpanZ = std::max(0.0001f, meshMax.z - meshMin.z);

    auto fitAxis = [](float value, float sourceMin, float sourceMax, float targetMin, float targetMax) -> float {
        float sourceSpan = std::max(0.0001f, sourceMax - sourceMin);
        float t = (value - sourceMin) / sourceSpan;
        return targetMin + t * (targetMax - targetMin);
    };

    auto pointAxisValue = [](const StorylandModelPoint& point, int axis) -> float {
        if (axis == 0) return point.x;
        if (axis == 1) return point.y;
        return point.z;
    };

    auto setPointAxisValue = [](StorylandModelPoint& point, int axis, float value) {
        if (axis == 0) point.x = value;
        else if (axis == 1) point.y = value;
        else point.z = value;
    };

    if (useZeroWorldFallbackOverlay) {
        // The corpus exposes two separate facts:
        //   1) LCS/PSP PEDs can have zero imported_global_0x50/LTM positions.
        //   2) The decoded mesh already has per-vertex skin weights.
        //
        // So for the viewport overlay, prefer actual skin-weight centroids.  This
        // places each display bone on the body part it influences, in real mesh
        // coordinates, and avoids the previous sideways/spider stick caused by
        // trying to reinterpret LCS local matrix bases in viewport space.
        std::vector<StorylandModelPoint> skinCentroids(bones.size());
        std::vector<float> skinTotals(bones.size(), 0.0f);

        std::map<uint32_t, uint32_t> boneIndexByDirectId;
        bool hasJawBone = false;
        bool allWorldPositionsAreZero = !bones.empty();
        for (uint32_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
            const StorylandModelBone& bone = bones[boneIndex];
            boneIndexByDirectId[bone.boneId] = boneIndex;

            std::string lowerName = bone.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (lowerName == "jaw" || bone.boneId == 8u) {
                hasJawBone = true;
            }

            if (bone.hasWorldPosition &&
                (std::fabs(bone.worldPosition.x) > 0.00001f ||
                 std::fabs(bone.worldPosition.y) > 0.00001f ||
                 std::fabs(bone.worldPosition.z) > 0.00001f)) {
                allWorldPositionsAreZero = false;
            }
        }

        bool useLcsDirectIdSkinPalette =
            bones.size() == 24u &&
            !hasJawBone &&
            allWorldPositionsAreZero &&
            boneIndexByDirectId.find(21u) != boneIndexByDirectId.end() &&
            boneIndexByDirectId.find(31u) != boneIndexByDirectId.end();

        auto lcsDirectBoneIdForSkinPaletteIndex = [](uint32_t paletteIndex) -> uint32_t {
            // LCS PS2 PED skin tokens follow the Atomic ped-bone hierarchy
            // table order.  In the uploaded LCS Cop.mdl, slot 6 is direct id
            // 31 (left clavicle), and slot 11 is direct id 21 (right clavicle).
            // So this is left-arm-first after head, not right-arm-first.
            static const uint32_t ids[] = {
                0u, 1u, 2u, 3u, 4u, 5u,
                31u, 32u, 33u, 34u, 35u,
                21u, 22u, 23u, 24u, 25u,
                41u, 42u, 43u, 54u,
                51u, 52u, 53u, 55u
            };
            if (paletteIndex >= (sizeof(ids) / sizeof(ids[0]))) return 0xFFFFFFFFu;
            return ids[paletteIndex];
        };

        auto lcsSkinPaletteIndexToBoneIndex = [&](uint32_t paletteIndex) -> uint32_t {
            if (!useLcsDirectIdSkinPalette) return 0xFFFFFFFFu;
            uint32_t directId = lcsDirectBoneIdForSkinPaletteIndex(paletteIndex);
            if (directId == 0xFFFFFFFFu) return 0xFFFFFFFFu;
            auto found = boneIndexByDirectId.find(directId);
            if (found == boneIndexByDirectId.end()) return 0xFFFFFFFFu;
            return found->second;
        };

        auto addWeightedSkinCentroid = [&](uint32_t boneIndex, const StorylandModelPoint& point, float weight) {
            skinCentroids[boneIndex].x += point.x * weight;
            skinCentroids[boneIndex].y += point.y * weight;
            skinCentroids[boneIndex].z += point.z * weight;
            skinTotals[boneIndex] += weight;
        };

        if (skinWeights.size() == points.size()) {
            for (size_t vertexIndex = 0; vertexIndex < points.size(); ++vertexIndex) {
                const StorylandModelPoint& point = points[vertexIndex];
                if (!pointIsFinite(point)) continue;

                const StorylandModelSkinWeights& weights = skinWeights[vertexIndex];
                if (!weights.valid) continue;

                for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount && influenceIndex < 4u; ++influenceIndex) {
                    const StorylandModelSkinInfluence& influence = weights.influences[influenceIndex];
                    if (influence.weight <= 0.00001f || !std::isfinite(influence.weight)) continue;

                    bool centroidAdded = false;

                    if (useLcsDirectIdSkinPalette) {
                        uint32_t lcsPaletteCandidates[2] = {
                            influence.boneIndex,
                            influence.rawMatrixIndex / 4u
                        };

                        for (uint32_t paletteIndex : lcsPaletteCandidates) {
                            uint32_t mappedBoneIndex = lcsSkinPaletteIndexToBoneIndex(paletteIndex);
                            if (mappedBoneIndex == 0xFFFFFFFFu || mappedBoneIndex >= bones.size()) continue;

                            addWeightedSkinCentroid(mappedBoneIndex, point, influence.weight);
                            centroidAdded = true;
                            break;
                        }
                    }

                    if (centroidAdded) continue;

                    uint32_t candidateIndices[3] = {
                        influence.boneIndex,
                        influence.rawMatrixIndex / 4u,
                        influence.rawMatrixIndex
                    };

                    for (uint32_t candidate : candidateIndices) {
                        if (candidate >= bones.size()) continue;

                        addWeightedSkinCentroid(candidate, point, influence.weight);
                        break;
                    }
                }
            }
        }

        uint32_t centroidCount = 0;
        for (uint32_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
            if (skinTotals[boneIndex] <= 0.00001f) continue;
            skinCentroids[boneIndex].x /= skinTotals[boneIndex];
            skinCentroids[boneIndex].y /= skinTotals[boneIndex];
            skinCentroids[boneIndex].z /= skinTotals[boneIndex];
            if (!pointIsFinite(skinCentroids[boneIndex])) continue;

            bones[boneIndex].hasComposedPosition = true;
            bones[boneIndex].composedPosition = skinCentroids[boneIndex];
            centroidCount++;
        }

        if (centroidCount >= std::max<uint32_t>(4u, uint32_t(bones.size() / 3u))) {
            // Fill missing parents from child centroids so root/pelvis/spine links
            // have visible endpoints even when those bones have few/no direct verts.
            for (uint32_t pass = 0; pass < 4u; ++pass) {
                for (uint32_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
                    if (bones[boneIndex].hasComposedPosition) continue;

                    StorylandModelPoint sum{};
                    uint32_t childCount = 0;
                    for (const auto& child : bones) {
                        if (child.parentIndex != boneIndex) continue;
                        if (!child.hasComposedPosition || !pointIsFinite(child.composedPosition)) continue;
                        sum = pointAdd(sum, child.composedPosition);
                        childCount++;
                    }

                    if (childCount > 0u) {
                        bones[boneIndex].hasComposedPosition = true;
                        bones[boneIndex].composedPosition = pointMul(sum, 1.0f / float(childCount));
                    }
                }
            }

            // Fill missing terminal bones from their parent chain.  The previous
            // version dropped no-direct-weight fingers/toes to the mesh center,
            // so hand->finger and foot->toe lines crossed the entire body and
            // looked like the arms were attached to the wrong bone.
            float meshLargestSpan = std::max(
                std::max(meshMax.x - meshMin.x, meshMax.y - meshMin.y),
                meshMax.z - meshMin.z
            );
            meshLargestSpan = std::max(0.0001f, meshLargestSpan);

            auto pointLength = [](const StorylandModelPoint& p) -> float {
                return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
            };
            auto pointSub = [](const StorylandModelPoint& a, const StorylandModelPoint& b) -> StorylandModelPoint {
                return StorylandModelPoint{a.x - b.x, a.y - b.y, a.z - b.z};
            };
            auto pointAddLocal = [](const StorylandModelPoint& a, const StorylandModelPoint& b) -> StorylandModelPoint {
                return StorylandModelPoint{a.x + b.x, a.y + b.y, a.z + b.z};
            };
            auto pointScale = [](const StorylandModelPoint& p, float scale) -> StorylandModelPoint {
                return StorylandModelPoint{p.x * scale, p.y * scale, p.z * scale};
            };

            for (uint32_t pass = 0; pass < 4u; ++pass) {
                for (uint32_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
                    StorylandModelBone& bone = bones[boneIndex];
                    if (bone.hasComposedPosition && pointIsFinite(bone.composedPosition)) continue;
                    if (bone.parentIndex == 0xFFFFFFFFu || bone.parentIndex >= bones.size()) continue;

                    const StorylandModelBone& parent = bones[bone.parentIndex];
                    if (!parent.hasComposedPosition || !pointIsFinite(parent.composedPosition)) continue;

                    StorylandModelPoint direction{};
                    bool haveDirection = false;
                    if (parent.parentIndex != 0xFFFFFFFFu && parent.parentIndex < bones.size()) {
                        const StorylandModelBone& grandParent = bones[parent.parentIndex];
                        if (grandParent.hasComposedPosition && pointIsFinite(grandParent.composedPosition)) {
                            direction = pointSub(parent.composedPosition, grandParent.composedPosition);
                            haveDirection = pointLength(direction) > 0.00001f;
                        }
                    }

                    if (!haveDirection) {
                        // Fall back to the local offset if the parent has no usable
                        // displayed parent.  This is mostly for root-adjacent nodes.
                        direction = bone.hasLocalPosition ? bone.localPosition : StorylandModelPoint{0.0f, 0.0f, 1.0f};
                        haveDirection = pointLength(direction) > 0.00001f;
                    }

                    float directionLength = std::max(0.00001f, pointLength(direction));
                    float localLength = bone.hasLocalPosition ? pointLength(bone.localPosition) : meshLargestSpan * 0.04f;
                    localLength = std::max(meshLargestSpan * 0.025f, std::min(localLength, meshLargestSpan * 0.12f));

                    bone.composedPosition = pointAddLocal(parent.composedPosition, pointScale(direction, localLength / directionLength));
                    bone.hasComposedPosition = true;
                }
            }

            StorylandModelPoint fallbackCenter{
                (meshMin.x + meshMax.x) * 0.5f,
                (meshMin.y + meshMax.y) * 0.5f,
                (meshMin.z + meshMax.z) * 0.5f
            };

            for (auto& bone : bones) {
                if (!bone.hasComposedPosition || !pointIsFinite(bone.composedPosition)) {
                    bone.composedPosition = fallbackCenter;
                    bone.hasComposedPosition = true;
                }

                bone.hasPreviewPosition = true;
                bone.previewPosition = bone.composedPosition;
                bone.previewPositionSource = useLcsDirectIdSkinPalette ? "LCS zero-world fallback: hierarchy-table skin palette centroids with parent-anchored terminal bones" : "LCS/PSP zero-world fallback: skin-weight centroid overlay with parent-anchored terminal bones";
            }
        } else {
            // Last-resort fallback: compose local matrices and fit axis-rank to the
            // mesh.  This is only used when no useful skin weights were decoded.
            std::vector<StorylandModelMatrix> composedMatrices(bones.size());
            std::vector<uint8_t> composedSolved(bones.size(), 0u);

            std::function<StorylandModelMatrix(uint32_t)> composeLocalMatrixForBone = [&](uint32_t boneIndex) -> StorylandModelMatrix {
                if (boneIndex >= bones.size()) return identityModelMatrix();
                if (composedSolved[boneIndex]) return composedMatrices[boneIndex];

                const StorylandModelBone& bone = bones[boneIndex];
                StorylandModelMatrix local = identityModelMatrix();
                auto nodeIt = offsetToNode.find(bone.offset);
                if (nodeIt != offsetToNode.end() && nodes[nodeIt->second].localMatrix.valid) {
                    local = nodes[nodeIt->second].localMatrix;
                }

                StorylandModelMatrix composed = local;
                if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size() && bone.parentIndex != boneIndex) {
                    composed = multiplyModelMatrix(composeLocalMatrixForBone(bone.parentIndex), local);
                }

                composedMatrices[boneIndex] = composed;
                composedSolved[boneIndex] = 1u;
                return composed;
            };

            for (uint32_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
                StorylandModelMatrix composedMatrix = composeLocalMatrixForBone(boneIndex);
                if (!composedMatrix.valid) continue;

                StorylandModelPoint composedPosition = matrixModelPosition(composedMatrix);
                if (!pointIsFinite(composedPosition)) continue;

                StorylandModelBone& bone = bones[boneIndex];
                bone.hasComposedPosition = true;
                bone.composedPosition = composedPosition;
            }

            StorylandModelPoint sourceMin{};
            StorylandModelPoint sourceMax{};
            bool haveSourceBounds = false;
            for (const auto& bone : bones) {
                if (!bone.hasComposedPosition || !pointIsFinite(bone.composedPosition)) continue;
                expandPointBounds(sourceMin, sourceMax, haveSourceBounds, bone.composedPosition);
            }
            if (!haveSourceBounds) return;

            float sourceSpans[3] = {
                std::max(0.0001f, sourceMax.x - sourceMin.x),
                std::max(0.0001f, sourceMax.y - sourceMin.y),
                std::max(0.0001f, sourceMax.z - sourceMin.z)
            };
            float targetSpans[3] = {meshSpanX, meshSpanY, meshSpanZ};

            int sourceAxes[3] = {0, 1, 2};
            int targetAxes[3] = {0, 1, 2};
            std::sort(std::begin(sourceAxes), std::end(sourceAxes), [&](int a, int b) {
                return sourceSpans[a] > sourceSpans[b];
            });
            std::sort(std::begin(targetAxes), std::end(targetAxes), [&](int a, int b) {
                return targetSpans[a] > targetSpans[b];
            });

            float sourceMinValues[3] = {sourceMin.x, sourceMin.y, sourceMin.z};
            float sourceMaxValues[3] = {sourceMax.x, sourceMax.y, sourceMax.z};
            float meshMinValues[3] = {meshMin.x, meshMin.y, meshMin.z};
            float meshMaxValues[3] = {meshMax.x, meshMax.y, meshMax.z};

            for (auto& bone : bones) {
                if (!bone.hasComposedPosition || !pointIsFinite(bone.composedPosition)) {
                    bone.hasPreviewPosition = false;
                    bone.previewPositionSource = "no zero-world fallback position for ped overlay";
                    continue;
                }

                StorylandModelPoint mapped{
                    (meshMin.x + meshMax.x) * 0.5f,
                    (meshMin.y + meshMax.y) * 0.5f,
                    (meshMin.z + meshMax.z) * 0.5f
                };

                for (int rank = 0; rank < 3; ++rank) {
                    int sourceAxis = sourceAxes[rank];
                    int targetAxis = targetAxes[rank];

                    float targetMin = meshMinValues[targetAxis];
                    float targetMax = meshMaxValues[targetAxis];
                    float border = (targetMax - targetMin) * 0.06f;

                    float fitted = fitAxis(
                        pointAxisValue(bone.composedPosition, sourceAxis),
                        sourceMinValues[sourceAxis],
                        sourceMaxValues[sourceAxis],
                        targetMin + border,
                        targetMax - border
                    );
                    setPointAxisValue(mapped, targetAxis, fitted);
                }

                bone.hasPreviewPosition = true;
                bone.previewPosition = mapped;
                bone.previewPositionSource = "LCS/PSP zero-world fallback: local matrices fitted to decoded mesh axes";
            }
        }
    } else {
        StorylandModelPoint boneMin{};
        StorylandModelPoint boneMax{};
        bool haveBoneBounds = false;
        for (const auto& bone : bones) {
            if (!bone.hasWorldPosition || !pointIsFinite(bone.worldPosition)) continue;
            expandPointBounds(boneMin, boneMax, haveBoneBounds, bone.worldPosition);
        }
        if (!haveBoneBounds) return;

        for (auto& bone : bones) {
            if (!bone.hasWorldPosition || !pointIsFinite(bone.worldPosition)) {
                bone.hasPreviewPosition = false;
                bone.previewPositionSource = "no imported_global_0x50 position for ped overlay";
                continue;
            }

            bone.hasPreviewPosition = true;
            bone.hasComposedPosition = true;
            bone.composedPosition = bone.worldPosition;
            bone.previewPosition = StorylandModelPoint{
                fitAxis(bone.worldPosition.x, boneMin.x, boneMax.x, meshMin.x, meshMax.x),
                fitAxis(bone.worldPosition.y, boneMin.y, boneMax.y, meshMin.y, meshMax.y),
                fitAxis(bone.worldPosition.z, boneMin.z, boneMax.z, meshMin.z, meshMax.z)
            };
            bone.previewPositionSource = "actual RslTAnim frame imported_global_0x50 fitted to robust decoded mesh bounds";
        }
    }

}


void StorylandModelFile::collectTextureNameHints() {
    textureHints.clear();
    std::set<std::string> seen;

    for (size_t offset = 0; offset < data.size();) {
        unsigned char ch = data[offset];
        if (ch < 32 || ch >= 127) {
            ++offset;
            continue;
        }

        size_t start = offset;
        while (offset < data.size()) {
            unsigned char c = data[offset];
            if (c < 32 || c >= 127) break;
            ++offset;
        }

        size_t length = offset - start;
        if (length >= 3 && length <= 64) {
            std::string text(reinterpret_cast<const char*>(data.data() + start), length);
            text = modelLowerAscii(text);
            if (modelLooksLikeTextureName(text) && seen.insert(text).second) {
                textureHints.push_back(text);
            }
        }
    }
}

void StorylandModelFile::parse() {
    outputLines.clear();
    fieldRows.clear();
    textureHints.clear();
    detectModelKind();
    collectTextureNameHints();
    collectPreviewPoints();
    collectArmatureBones();

    std::ostringstream ss;
    ss << "MDL size: " << data.size() << " bytes / " << ((data.size() + 2047) / 2048) << " IMG sectors";
    outputLines.push_back({ss.str()});
    outputLines.push_back({std::string("Auto-detected model type: ") + modelKindName()});
    {
        std::ostringstream armatureLine;
        if (kind == StorylandModelKind::PedModel || kind == StorylandModelKind::CutsceneModel) {
            armatureLine << "Armature imported: " << bones.size() << " RslTAnim/RslNode frame-bone entries";
            if (!bones.empty()) {
                uint32_t parented = 0;
                for (const auto& bone : bones) {
                    if (bone.parentIndex != 0xFFFFFFFFu) parented++;
                }
                armatureLine << " (" << parented << " parent links resolved)";
            }
            armatureLine << ".";
            uint32_t rotationBasisCount = 0;
            uint32_t localRotationBasisCount = 0;
            uint32_t canonicalIdCount = 0;
            for (const auto& bone : bones) {
                if (bone.hasWorldRotation) rotationBasisCount++;
                if (bone.hasLocalRotation) localRotationBasisCount++;
                if (bone.boneId != 0xFFFFFFFFu && bone.boneId != 255u) canonicalIdCount++;
            }
            if (rotationBasisCount > 0) {
                armatureLine << " World bind rotation bases=" << rotationBasisCount << ".";
            }
            if (localRotationBasisCount > 0) {
                armatureLine << " Local bind rotation bases=" << localRotationBasisCount << ".";
            }
            armatureLine << " direct/skin-aware anim bone ids resolved=" << canonicalIdCount << "/" << bones.size() << ".";
        } else {
            armatureLine << "Armature import skipped: " << modelKindName() << " does not use the ped/cutscene RslTAnim skeleton path.";
        }
        outputLines.push_back({armatureLine.str()});
    }
    if (!textureHints.empty()) {
        std::ostringstream textureLine;
        textureLine << "Texture name hints embedded in MDL: ";
        for (size_t index = 0; index < textureHints.size() && index < 12; ++index) {
            if (index) textureLine << ", ";
            textureLine << textureHints[index];
        }
        if (textureHints.size() > 12) textureLine << ", ...";
        outputLines.push_back({textureLine.str()});
    } else {
        outputLines.push_back({"Texture name hints embedded in MDL: none found."});
    }
    outputLines.push_back({"Left pane is a named struct tree: sChunkHeader, Top-level payload, RslElementGroup/Clump, RslElement/Atomic, RslNode1/2/3, and pointer candidates."});

    if (data.size() >= 0x28) {
        for (size_t offset = 0; offset < 0x40 && offset + 4 <= data.size(); offset += 4) {
            uint32_t value = modelReadU32(data, offset);
            std::string group = offset < 0x20 ? "sChunkHeader" : "Top-level payload";
            std::string note;
            if (offset == 0x00) note = "Expected MDL_IDENT = 0x006D646C / bytes 'l d m 00'.";
            else if (offset == 0x08) note = "Full chunk size; should match the MDL file size.";
            else if (offset == 0x0C) note = "Local relocation table offset.";
            else if (offset == 0x10) note = "Global relocation/pointer table offset.";
            else if (offset == 0x14) note = "Number of relocation entries.";
            else if (offset == 0x1C) note = "Allocated memory budget from chunk header.";
            else if (offset == 0x20) note = "Top-level slot 0. PedData.colModel, direct ElementGroup pointer, Atomic pointer, or family-specific data.";
            else if (offset == 0x24) note = "Top-level slot 1. PedData.elementgroup for peds, or family-specific data.";
            if (modelPointerLooksValid(data, value)) {
                if (!note.empty()) note += " ";
                note += "Valid file offset -> first dword " + modelHex32(modelReadU32(data, value)) + ".";
            }
            addField(fieldRows, group, knownMdlHeaderName(offset), uint32_t(offset), value, note);
        }

        uint32_t slot0 = modelReadU32(data, 0x20);
        uint32_t slot1 = modelReadU32(data, 0x24);
        if (looksLikeClumpAt(data, slot1)) {
            addField(fieldRows, "Resolved model family", "LoadPed candidate", 0x24, slot1, "slot1 points to RslElementGroup / Clump. Treat top-level as PedData { colModel, elementgroup }.");
        } else if (looksLikeClumpAt(data, slot0)) {
            addField(fieldRows, "Resolved model family", "LoadElementGroup candidate", 0x20, slot0, "slot0 points directly to RslElementGroup / Clump.");
        } else if (looksLikeAtomicAt(data, slot0)) {
            addField(fieldRows, "Resolved model family", "LoadSimple candidate", 0x20, slot0, "slot0 points to an RslElement / Atomic entry.");
        } else {
            addField(fieldRows, "Resolved model family", "unresolved top-level layout", 0x20, slot0, "Storyland could not resolve the first payload slots to a known Clump/Atomic layout yet.");
        }
    }

    uint32_t clumpIndex = 0;
    uint32_t atomicIndex = 0;
    uint32_t node1Index = 0;
    uint32_t node2Index = 0;
    uint32_t node3Index = 0;
    uint32_t genericAaIndex = 0;

    for (size_t offset = 0; offset + 4 <= data.size(); offset += 4) {
        uint32_t value = modelReadU32(data, offset);
        std::string kindName = classifySectionId(value);
        if (kindName.empty()) continue;

        if (kindName == "Clump") {
            std::ostringstream group; group << "Clump #" << clumpIndex++ << " @ " << modelHexOffset(offset);
            addField(fieldRows, group.str(), "section_id", uint32_t(offset + 0x00), value, sectionValueBytes(value) + " / RslElementGroup Clump");
            addPointerField(data, fieldRows, group.str(), "root_frame_ptr", uint32_t(offset + 0x04), "Expected to point to root/scene_root RslNode1 for normal MDLs");
            addPointerField(data, fieldRows, group.str(), "clump_atomic_cycle_next", uint32_t(offset + 0x08), "Cycle link to first atomic");
            addPointerField(data, fieldRows, group.str(), "clump_atomic_cycle_prev", uint32_t(offset + 0x0C), "Cycle link to last/previous atomic");
            addField(fieldRows, group.str(), "object.type/subType/flags/privateFlags", uint32_t(offset + 0x00), value, "RslElementGroup is the Leeds/RSL Clump container.");
        } else if (kindName == "Atomic") {
            std::ostringstream group; group << "Atomic #" << atomicIndex++ << " @ " << modelHexOffset(offset);
            addField(fieldRows, group.str(), "section_id", uint32_t(offset + 0x00), value, sectionValueBytes(value) + " / RslElement Atomic");
            addPointerField(data, fieldRows, group.str(), "parent_frame_ptr", uint32_t(offset + 0x04), "Atomic parent frame");
            addPointerField(data, fieldRows, group.str(), "frame_atomic_cycle_next", uint32_t(offset + 0x08), "Frame atomic cycle next");
            addPointerField(data, fieldRows, group.str(), "frame_atomic_cycle_prev", uint32_t(offset + 0x0C), "Frame atomic cycle prev");
            addField(fieldRows, group.str(), "filler_or_flags", uint32_t(offset + 0x10), modelReadU32(data, offset + 0x10), "Often AAAAAAAA or runtime filler");
            addPointerField(data, fieldRows, group.str(), "geometry_ptr", uint32_t(offset + 0x14), "Pointer to Leeds geometry wrapper/header");
            addPointerField(data, fieldRows, group.str(), "clump_ptr", uint32_t(offset + 0x18), "Back pointer to clump");
            addPointerField(data, fieldRows, group.str(), "clump_atomic_cycle_next", uint32_t(offset + 0x1C), "Clump atomic cycle next");
            addPointerField(data, fieldRows, group.str(), "clump_atomic_cycle_prev", uint32_t(offset + 0x20), "Clump atomic cycle prev");
            addField(fieldRows, group.str(), "render_callback_or_pipeline", uint32_t(offset + 0x24), modelReadU32(data, offset + 0x24), "Render callback/pipeline id field");
            addField(fieldRows, group.str(), "ide_id_or_runtime", uint32_t(offset + 0x28), modelReadU32(data, offset + 0x28), "Ped MDLs often use FFFF or engine-filled value");
            addPointerField(data, fieldRows, group.str(), "ped_bone_params_ptr", uint32_t(offset + 0x2C), "Ped/cutscene bone params pointer when present");
            addField(fieldRows, group.str(), "unknown_0x30", uint32_t(offset + 0x30), modelReadU32(data, offset + 0x30), "Atomic trailing field");
        } else if (kindName == "RslNode1" || kindName == "RslNode2" || kindName == "RslNode3") {
            uint32_t index = 0;
            if (kindName == "RslNode1") index = node1Index++;
            else if (kindName == "RslNode2") index = node2Index++;
            else index = node3Index++;
            std::ostringstream group; group << kindName << " #" << index << " @ " << modelHexOffset(offset);
            addField(fieldRows, group.str(), "section_id", uint32_t(offset + 0x00), value, sectionValueBytes(value) + " / " + mdlSectionMeaning(value));
            addPointerField(data, fieldRows, group.str(), "parent_or_root_ptr", uint32_t(offset + 0x04), "Frame parent/root pointer candidate.");
            addPointerField(data, fieldRows, group.str(), "object_or_frame_cycle_next", uint32_t(offset + 0x08), "RslLinkList/objectList or child/cycle link candidate.");
            addPointerField(data, fieldRows, group.str(), "object_or_frame_cycle_prev", uint32_t(offset + 0x0C), "RslLinkList/objectList or sibling/cycle link candidate.");
            addMatrixFields(data, fieldRows, group.str(), uint32_t(offset + 0x10), "modelling");
            addMatrixFields(data, fieldRows, group.str(), uint32_t(offset + 0x50), "ltm_or_global");
            if (offset + 0xA0 <= data.size()) {
                addPointerField(data, fieldRows, group.str(), "child_ptr", uint32_t(offset + 0x90), "RslNode.child candidate after modelling/ltm matrices");
                addPointerField(data, fieldRows, group.str(), "next_ptr", uint32_t(offset + 0x94), "RslNode.next sibling candidate");
                addPointerField(data, fieldRows, group.str(), "root_ptr", uint32_t(offset + 0x98), "RslNode.root candidate");
                addField(fieldRows, group.str(), "nodeId", uint32_t(offset + 0x9C), modelReadU32(data, offset + 0x9C), "RwHAnim/RslTAnim node id or variant field");
            }
        } else {
            genericAaIndex++;
        }
    }

    outputLines.push_back({""});
    std::ostringstream scanLine;
    scanLine << "Struct scan found clumps=" << clumpIndex
             << ", atomics=" << atomicIndex
             << ", RslNode1=" << node1Index
             << ", RslNode2=" << node2Index
             << ", RslNode3=" << node3Index
             << ". Hidden non-struct AA/padding words=" << genericAaIndex << ".";
    outputLines.push_back({scanLine.str()});

    uint32_t pointerCount = 0;
    for (size_t offset = 0; offset + 4 <= data.size(); offset += 4) {
        uint32_t value = modelReadU32(data, offset);
        if (modelPointerLooksValid(data, value)) {
            std::ostringstream group; group << "Pointer Table Candidates";
            std::ostringstream name; name << "ptr_at_" << modelHexOffset(offset);
            std::string note = "points to " + modelHexOffset(value) + ", first dword " + modelHex32(modelReadU32(data, value));
            addField(fieldRows, group.str(), name.str(), uint32_t(offset), value, note);
            pointerCount++;
            if (pointerCount >= 256) break;
        }
    }

    std::ostringstream pointLine;
    pointLine << "Geometry: " << points.size() << " vertices, " << triangles.size() << " triangles";
    outputLines.insert(outputLines.begin() + std::min<size_t>(2, outputLines.size()), {pointLine.str()});
}

const std::vector<StorylandModelLine>& StorylandModelFile::lines() const { return outputLines; }
const std::vector<StorylandModelPoint>& StorylandModelFile::previewPoints() const { return points; }
const std::vector<StorylandModelTriangle>& StorylandModelFile::previewTriangles() const { return triangles; }
const std::vector<StorylandModelTexcoord>& StorylandModelFile::previewTexcoords() const { return texcoords; }
const std::vector<StorylandModelSkinWeights>& StorylandModelFile::previewSkinWeights() const { return skinWeights; }
const std::vector<StorylandModelBone>& StorylandModelFile::armatureBones() const { return bones; }
const std::vector<std::string>& StorylandModelFile::previewMaterialTextureNames() const { return materialTextureNames; }
const std::vector<std::string>& StorylandModelFile::textureNameHints() const { return textureHints; }
const std::wstring& StorylandModelFile::sourcePath() const { return path; }
size_t StorylandModelFile::fileSize() const { return data.size(); }
const std::vector<StorylandModelField>& StorylandModelFile::fields() const { return fieldRows; }
StorylandModelKind StorylandModelFile::modelKind() const { return kind; }
std::string StorylandModelFile::modelKindName() const {
    switch (kind) {
    case StorylandModelKind::SimpleModel: return "SimpleModel";
    case StorylandModelKind::PedModel: return "PedModel";
    case StorylandModelKind::CutsceneModel: return "CutsceneModel";
    case StorylandModelKind::VehicleModel: return "VehicleModel";
    case StorylandModelKind::WorldModel: return "WorldModel";
    default: return "Unknown";
    }
}
