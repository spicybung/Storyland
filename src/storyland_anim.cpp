#include "storyland_anim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

static uint16_t animReadU16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
}

static int16_t animReadI16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<int16_t>(animReadU16(data, offset));
}

static uint32_t animReadU32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return uint32_t(data[offset]) |
           (uint32_t(data[offset + 1]) << 8) |
           (uint32_t(data[offset + 2]) << 16) |
           (uint32_t(data[offset + 3]) << 24);
}

static float animReadF32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0.0f;
    float value = 0.0f;
    std::memcpy(&value, data.data() + offset, 4);
    return value;
}

static std::string animHexOffset(uint32_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << value;
    return ss.str();
}

static std::string animHex32(uint32_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return ss.str();
}

static bool animReadWholeFile(const std::wstring& filePath, std::vector<uint8_t>& bytes, std::string& errorMessage) {
#ifdef _WIN32
    FILE* file = nullptr;
    if (_wfopen_s(&file, filePath.c_str(), L"rb") != 0 || file == nullptr) {
        errorMessage = "Could not open .anim file.";
        return false;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        errorMessage = "Could not determine .anim file size.";
        return false;
    }
    bytes.resize(size_t(size));
    if (!bytes.empty()) {
        size_t readCount = fread(bytes.data(), 1, bytes.size(), file);
        if (readCount != bytes.size()) {
            fclose(file);
            errorMessage = "Could not read full .anim file.";
            return false;
        }
    }
    fclose(file);
    return true;
#else
    std::string narrow(filePath.begin(), filePath.end());
    std::ifstream file(narrow, std::ios::binary);
    if (!file) {
        errorMessage = "Could not open .anim file.";
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 0) {
        errorMessage = "Could not determine .anim file size.";
        return false;
    }
    bytes.resize(size_t(size));
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file && size != 0) {
        errorMessage = "Could not read full .anim file.";
        return false;
    }
    return true;
#endif
}

static std::string animLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& ch : out) {
        if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
    }
    return out;
}

static bool animLooksLikeUsefulString(const std::string& text) {
    if (text.size() < 2 || text.size() > 64) return false;
    bool hasLetter = false;
    for (char ch : text) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) hasLetter = true;
        if (!(ch >= 32 && ch < 127)) return false;
    }
    return hasLetter;
}

static std::string animReadFixedString(const std::vector<uint8_t>& data, size_t offset, size_t maxBytes) {
    std::string out;
    for (size_t i = 0; i < maxBytes && offset + i < data.size(); ++i) {
        uint8_t ch = data[offset + i];
        if (ch == 0) break;
        if (ch < 32 || ch >= 127) break;
        out.push_back(char(ch));
    }
    return out;
}

static bool animValidPointer(const std::vector<uint8_t>& data, uint32_t offset) {
    return offset < data.size();
}

static uint32_t animDataEndGuess(const std::vector<uint8_t>& data) {
    uint32_t best = uint32_t(data.size());
    uint32_t localReloc = animReadU32(data, 0x0C);
    uint32_t globalReloc = animReadU32(data, 0x10);
    if (localReloc > 0x20 && localReloc <= data.size()) best = std::min(best, localReloc);
    if (globalReloc > 0x20 && globalReloc <= data.size()) best = std::min(best, globalReloc);
    return best;
}

static const std::vector<std::string>& animCanonicalBoneNames() {
    // Direct/HAnim display order for Leeds LCS/VCS PED/Cutscene animation.
    // LCS PS2 retail ANIMs commonly contain 24/25 channels:
    // body, left arm, right arm, left leg/toe, right leg/toe, and sometimes jaw/extra.
    static const std::vector<std::string> names = {
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
    return names;
}

static uint32_t animCanonicalParent(uint32_t index) {
    switch (index) {
    case 0: return 0xFFFFFFFFu; // root
    case 1: return 0;           // pelvis -> root
    case 2: return 1;           // spine -> pelvis
    case 3: return 2;           // spine1 -> spine
    case 4: return 3;           // neck -> spine1
    case 5: return 4;           // head -> neck
    case 6: return 5;           // jaw -> head
    case 7: return 3;           // l clavicle -> spine1
    case 8: return 7;
    case 9: return 8;
    case 10: return 9;
    case 11: return 10;
    case 12: return 3;          // r clavicle -> spine1
    case 13: return 12;
    case 14: return 13;
    case 15: return 14;
    case 16: return 15;
    case 17: return 1;          // l thigh -> pelvis
    case 18: return 17;
    case 19: return 18;
    case 20: return 19;
    case 21: return 1;          // r thigh -> pelvis
    case 22: return 21;
    case 23: return 22;
    case 24: return 23;
    default: return 0xFFFFFFFFu;
    }
}

static uint32_t animBoneIdForCanonicalIndex(uint32_t index) {
    static const uint32_t ids[] = {
        0, 1, 2, 3, 4, 5, 8,
        31, 32, 33, 34, 35,
        21, 22, 23, 24, 25,
        41, 42, 43, 54,
        51, 52, 53, 55
    };
    if (index < sizeof(ids) / sizeof(ids[0])) return ids[index];
    return 0xFFFFFFFFu;
}

static uint32_t animCanonicalIndexForBoneId(uint32_t boneId) {
    const auto& names = animCanonicalBoneNames();
    for (uint32_t index = 0; index < names.size(); ++index) {
        if (animBoneIdForCanonicalIndex(index) == boneId) return index;
    }

    // VCS display-only toe ids used by some Storyland/BLeeds paths.
    if (boneId == 2000u) return 20u;
    if (boneId == 2001u) return 24u;

    return 0xFFFFFFFFu;
}

static uint32_t animBoneIdFromChannelTag(uint32_t tag) {
    uint32_t upper = (tag >> 16) & 0xFFFFu;
    uint32_t lower = tag & 0xFFFFu;

    // Some anim banks store the HAnim/RslTAnim bone id directly in the low 16 bits:
    //   0xDF9E0000 -> bone 0
    //   0xB88A0001 -> bone 1
    // Other banks, including weapon anims, keep the stable channel hash in the high
    // 16 bits and use the low 16 bits for extra channel data:
    //   0xDF9EEB10, 0xB88AC017, ...
    // So decode the known high-word channel ids first, then fall back to plausible
    // direct low-word bone ids.
    switch (upper) {
    // BLeeds PED_ANIM_HASH16_TO_DIRECT_ID bridge.
    // These are animation direct/HAnim ids, not compact hierarchy node indices.
    case 0xDF9E: return 0;   // root
    case 0xB88A: return 1;   // pelvis
    case 0x9A64: return 2;   // spine
    case 0xBCFC: return 3;   // spine1
    case 0xE97B: return 4;   // neck
    case 0x6E99: return 5;   // head
    case 0x8935: return 21;  // bip01_r_clavicle
    case 0x1157: return 22;  // r_upperarm
    case 0x7A50: return 23;  // r_forearm
    case 0xFE09: return 24;  // r_hand
    case 0xC8F4: return 25;  // r_finger / related bank
    case 0x5E16: return 25;  // r_finger / related bank alias
    case 0xB052: return 31;  // bip01_l_clavicle
    case 0x2830: return 32;  // l_upperarm
    case 0x1E52: return 33;  // l_forearm
    case 0xC7D5: return 34;  // l_hand
    case 0xA1C5: return 35;  // l_finger
    case 0x3A14: return 35;  // l_finger / related bank alias
    case 0xCDD3: return 41;  // l_thigh
    case 0xCCEC: return 42;  // l_calf
    case 0x2938: return 43;  // l_foot
    case 0x6318: return 54;  // l_toe0, LCS PS2 weapon/bike banks
    case 0xFA32: return 51;  // r_thigh
    case 0xF530: return 52;  // r_calf
    case 0x10E4: return 53;  // r_foot
    case 0x5AC4: return 55;  // r_toe0, LCS PS2 weapon/bike banks
    case 0x80DF: return 8;   // jaw/extra head channel seen in 25-channel LCS banks
    default: break;
    }

    // Direct low-word IDs are valid for some non-weapon anim banks.
    // Weapon banks above must be decoded through the high-word hash first.
    if (lower <= 80u || lower == 255u) return lower;
    return 0xFFFFFFFFu;
}

static bool animChannelTypeIsKnown(uint32_t channelType) {
    // Leeds ANIM channel low bits:
    //   bit 0 = rotation half4
    //   bit 1 = translation half3
    //   bit 2 = scale half3
    // Extra bank/platform bits are seen in retail files:
    //   0x08 = VCS/PSP-style extra flag
    //   0x10 = PS2 LCS / bikeh/sword-style extra flag
    //
    // Examples:
    //   ak47  uses 0x01 / 0x03
    //   strip uses 0x09 / 0x0B
    //   bikeh and sword use 0x11 / 0x13
    if ((channelType & 0x0001u) == 0) {
        return false;
    }

    if ((channelType & ~0x001Fu) != 0) {
        return false;
    }

    // Scale is documented, but not used by the known LCS/VCS retail ANIMs here.
    // Keep it readable if it appears, but reject nonsensical flag combinations.
    return true;
}

static uint32_t animStrideForChannelType(uint32_t channelType) {
    uint32_t stride = 0;
    if ((channelType & 0x0001u) != 0) stride += 8; // rotation quaternion half4
    stride += 2;                                  // per-key time interval half
    if ((channelType & 0x0002u) != 0) stride += 6; // translation half3
    if ((channelType & 0x0004u) != 0) stride += 6; // scale half3, rarely/never used
    return stride;
}

static std::string animNameForBoneId(uint32_t boneId, uint32_t fallbackIndex) {
    const auto& names = animCanonicalBoneNames();

    for (uint32_t i = 0; i < names.size(); ++i) {
        if (animBoneIdForCanonicalIndex(i) == boneId && boneId != 255u) return names[i];
    }

    if (fallbackIndex < names.size()) return names[fallbackIndex];

    std::ostringstream ss;
    ss << "bone_id_" << boneId;
    return ss.str();
}

static bool animFloatLooksSane(float value, float limit) {
    return std::isfinite(value) && value >= -limit && value <= limit;
}

static void normalizeQuat(float& x, float& y, float& z, float& w) {
    float length = std::sqrt(x * x + y * y + z * z + w * w);
    if (!std::isfinite(length) || length < 0.00001f) {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 1.0f;
        return;
    }

    x /= length;
    y /= length;
    z /= length;
    w /= length;
}

static float animHalfToFloat(uint16_t value) {
    uint32_t sign = (uint32_t(value & 0x8000u)) << 16;
    uint32_t exponent = (value >> 10) & 0x1Fu;
    uint32_t mantissa = value & 0x03FFu;

    uint32_t result = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            result = sign;
        } else {
            exponent = 1;
            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x03FFu;
            uint32_t floatExponent = exponent + (127u - 15u);
            result = sign | (floatExponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 31u) {
        result = sign | 0x7F800000u | (mantissa << 13);
    } else {
        uint32_t floatExponent = exponent + (127u - 15u);
        result = sign | (floatExponent << 23) | (mantissa << 13);
    }

    float out = 0.0f;
    std::memcpy(&out, &result, sizeof(out));
    return out;
}

static float animReadHalf(const std::vector<uint8_t>& data, size_t offset) {
    return animHalfToFloat(animReadU16(data, offset));
}

static StorylandAnimKey decodeAnimKeyWithFlags(const std::vector<uint8_t>& data, uint32_t offset, uint32_t flags) {
    StorylandAnimKey key;
    key.offset = offset;

    uint32_t cursor = 0;

    if ((flags & 0x0001u) != 0) {
        key.qx = animReadHalf(data, offset + cursor + 0);
        key.qy = animReadHalf(data, offset + cursor + 2);
        key.qz = animReadHalf(data, offset + cursor + 4);
        key.qw = animReadHalf(data, offset + cursor + 6);
        cursor += 8;
        normalizeQuat(key.qx, key.qy, key.qz, key.qw);
    }

    // The GTAModding.ru ANIM notes label this as "time * 30", but in the
    // shipped data this half-float is the per-key frame interval in seconds:
    // 0.03333 = 1/30 sec, 0.06666 = 2/30 sec, and the interval sum matches
    // the clip duration. We keep seconds here and accumulate it later.
    key.time = animReadHalf(data, offset + cursor);
    cursor += 2;

    if ((flags & 0x0002u) != 0) {
        key.tx = animReadHalf(data, offset + cursor + 0);
        key.ty = animReadHalf(data, offset + cursor + 2);
        key.tz = animReadHalf(data, offset + cursor + 4);
        cursor += 6;
    }

    if ((flags & 0x0004u) != 0) {
        // Scale exists in the documented generic frame layout, but LCS/VCS
        // retail files normally do not use it. Skip it if encountered.
        cursor += 6;
    }

    if (!animFloatLooksSane(key.qx, 4.0f) || !animFloatLooksSane(key.qy, 4.0f) ||
        !animFloatLooksSane(key.qz, 4.0f) || !animFloatLooksSane(key.qw, 4.0f)) {
        key.qx = 0.0f;
        key.qy = 0.0f;
        key.qz = 0.0f;
        key.qw = 1.0f;
    }

    if (!animFloatLooksSane(key.time, 120.0f)) key.time = 0.0f;

    if (!animFloatLooksSane(key.tx, 64.0f) || !animFloatLooksSane(key.ty, 64.0f) || !animFloatLooksSane(key.tz, 64.0f)) {
        key.tx = 0.0f;
        key.ty = 0.0f;
        key.tz = 0.0f;
    }

    return key;
}

static StorylandAnimPoseBone animBasePoseBone(uint32_t index) {
    const auto& names = animCanonicalBoneNames();

    auto makePoint = [&](uint32_t boneIndex, float x, float y, float z, bool visible = true) {
        StorylandAnimPoseBone bone;
        bone.index = boneIndex;
        bone.parentIndex = animCanonicalParent(boneIndex);
        bone.boneId = animBoneIdForCanonicalIndex(boneIndex);
        bone.name = boneIndex < names.size() ? names[boneIndex] : "extra_track";
        bone.x = x;
        bone.y = y;
        bone.z = z;
        bone.visible = visible;
        return bone;
    };

    switch (index) {
    case 0: return makePoint(index, 0.00f, 0.00f, 0.00f, false);
    case 1: return makePoint(index, 0.00f, 0.00f, 0.55f);
    case 2: return makePoint(index, 0.00f, 0.00f, 0.82f);
    case 3: return makePoint(index, 0.00f, 0.00f, 1.08f);
    case 4: return makePoint(index, 0.00f, 0.00f, 1.24f);
    case 5: return makePoint(index, 0.00f, 0.00f, 1.38f);
    case 6: return makePoint(index, 0.00f, -0.04f, 1.33f);

    case 7: return makePoint(index, 0.10f, 0.00f, 1.17f);
    case 8: return makePoint(index, 0.31f, 0.00f, 1.08f);
    case 9: return makePoint(index, 0.52f, 0.00f, 0.96f);
    case 10: return makePoint(index, 0.67f, 0.00f, 0.88f);
    case 11: return makePoint(index, 0.73f, 0.00f, 0.84f);

    case 12: return makePoint(index, -0.10f, 0.00f, 1.17f);
    case 13: return makePoint(index, -0.31f, 0.00f, 1.08f);
    case 14: return makePoint(index, -0.52f, 0.00f, 0.96f);
    case 15: return makePoint(index, -0.67f, 0.00f, 0.88f);
    case 16: return makePoint(index, -0.73f, 0.00f, 0.84f);

    case 17: return makePoint(index, 0.13f, 0.00f, 0.50f);
    case 18: return makePoint(index, 0.14f, 0.00f, 0.25f);
    case 19: return makePoint(index, 0.14f, -0.05f, 0.04f);
    case 20: return makePoint(index, 0.14f, -0.17f, 0.02f);

    case 21: return makePoint(index, -0.13f, 0.00f, 0.50f);
    case 22: return makePoint(index, -0.14f, 0.00f, 0.25f);
    case 23: return makePoint(index, -0.14f, -0.05f, 0.04f);
    case 24: return makePoint(index, -0.14f, -0.17f, 0.02f);
    default: break;
    }

    StorylandAnimPoseBone bone;
    bone.index = index;
    bone.parentIndex = 0xFFFFFFFFu;
    bone.boneId = 0xFFFFFFFFu;
    bone.name = "extra_track";
    bone.visible = false;
    return bone;
}


bool StorylandAnimFile::loadFromFile(const std::wstring& filePath, std::string& errorMessage) {
    path = filePath;
    data.clear();
    fieldRows.clear();
    trackRows.clear();
    clipRows.clear();
    strings.clear();
    duration = 1.0f;
    fps = 30.0f;
    frames = 30;
    activeClip = 0;
    hasCandidates = false;

    if (!animReadWholeFile(filePath, data, errorMessage)) return false;
    if (data.empty()) {
        errorMessage = ".anim file is empty.";
        return false;
    }

    parse();
    return true;
}

void StorylandAnimFile::parse() {
    collectStrings();
    collectHeaderFields();
    scanLeedsAnimationClips();
    if (trackRows.empty()) buildStaticInspectionTracks();

    uint32_t maxKeys = 1;
    for (const auto& track : trackRows) {
        maxKeys = std::max<uint32_t>(maxKeys, uint32_t(track.keys.size()));
    }

    frames = std::max<uint32_t>(1, maxKeys);
    if (!clipRows.empty() && clipRows[activeClip].duration > 0.0f) {
        duration = clipRows[activeClip].duration;
    } else {
        duration = std::max(1.0f / fps, float(std::max<uint32_t>(1, frames - 1)) / fps);
    }
}

void StorylandAnimFile::collectStrings() {
    std::set<std::string> seen;
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
            if (value.size() > 96) break;
        }

        if (offset < data.size() && data[offset] == 0 && animLooksLikeUsefulString(value)) {
            std::string lower = animLowerAscii(value);
            if (seen.insert(lower).second) strings.push_back(value);
        }

        offset = std::max(offset + 1, start + 1);
    }
}

void StorylandAnimFile::collectHeaderFields() {
    size_t dwordCount = std::min<size_t>(data.size() / 4, 32);
    fieldRows.reserve(dwordCount + 16);

    StorylandAnimField sizeField;
    sizeField.group = "File";
    sizeField.name = "byte_size";
    sizeField.offset = 0;
    sizeField.value = uint32_t(std::min<size_t>(data.size(), 0xFFFFFFFFu));
    sizeField.note = "Raw .anim byte size.";
    fieldRows.push_back(sizeField);

    for (size_t index = 0; index < dwordCount; ++index) {
        uint32_t offset = uint32_t(index * 4);
        uint32_t value = animReadU32(data, offset);
        StorylandAnimField field;
        field.group = "Raw header probe";
        field.name = "dword_" + std::to_string(index);
        field.offset = offset;
        field.value = value;

        float asFloat = animReadF32(data, offset);
        std::ostringstream note;
        note << animHex32(value);
        if (offset == 0x00 && value == 0x616E696Du) note << " / 'anim' magic stored little-endian as bytes m i n a";
        if (offset == 0x0C || offset == 0x10) note << " / animation data end or relocation/table pointer candidate";
        if (offset == 0x14) note << " / top pointer table byte count candidate";
        if (std::isfinite(asFloat) && asFloat > -100000.0f && asFloat < 100000.0f) {
            note << " / float=" << asFloat;
        }
        field.note = note.str();
        fieldRows.push_back(field);
    }
}

void StorylandAnimFile::scanLeedsAnimationClips() {
    if (data.size() < 0x24) return;
    if (animReadU32(data, 0x00) != 0x616E696Du) return;

    uint32_t tableByteCount = animReadU32(data, 0x14);
    if (tableByteCount == 0 || tableByteCount > 0x4000u) return;

    uint32_t pointerCount = tableByteCount / 4u;
    if (0x20u + pointerCount * 4u > data.size()) {
        pointerCount = uint32_t((data.size() - 0x20u) / 4u);
    }

    // Some banks store a much larger relocation count at 0x14.
    // If the first pointer lands immediately after the pointer table, that
    // gives the real clip count directly.  Keep the 0x14 count for tail-table
    // banks such as strip.anim, where the first clip entry lives near EOF.
    uint32_t firstEntryOffset = animReadU32(data, 0x20);
    if (firstEntryOffset >= 0x20u &&
        firstEntryOffset <= 0x4000u &&
        ((firstEntryOffset - 0x20u) % 4u) == 0u) {
        uint32_t countFromFirstEntry = (firstEntryOffset - 0x20u) / 4u;
        if (countFromFirstEntry > 0 && countFromFirstEntry < pointerCount) {
            pointerCount = countFromFirstEntry;
        }
    }

    std::set<uint32_t> seenEntryOffsets;
    for (uint32_t pointerIndex = 0; pointerIndex < pointerCount; ++pointerIndex) {
        uint32_t entryOffset = animReadU32(data, 0x20u + pointerIndex * 4u);
        if (!animValidPointer(data, entryOffset) || entryOffset + 0x24u > data.size()) continue;
        if (!seenEntryOffsets.insert(entryOffset).second) continue;

        uint32_t channelTableOffset = animReadU32(data, entryOffset + 0x00);
        std::string name = animReadFixedString(data, entryOffset + 0x04, 24);
        uint32_t channelCount = animReadU16(data, entryOffset + 0x1C);
        uint32_t flags = animReadU16(data, entryOffset + 0x1E);
        float clipDuration = animReadF32(data, entryOffset + 0x20);

        if (!animLooksLikeUsefulString(name)) continue;
        if (channelCount == 0 || channelCount > 160) continue;
        if (!std::isfinite(clipDuration) || clipDuration <= 0.0f || clipDuration > 120.0f) continue;
        if (!animValidPointer(data, channelTableOffset) || channelTableOffset + channelCount * 12u > data.size()) continue;

        uint32_t validChannels = 0;
        for (uint32_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
            size_t descriptorOffset = size_t(channelTableOffset) + size_t(channelIndex) * 12u;
            uint32_t keyCount = animReadU16(data, descriptorOffset + 2);
            uint32_t keyOffset = animReadU32(data, descriptorOffset + 4);
            if (keyCount > 0 && keyCount < 1000 && animValidPointer(data, keyOffset)) validChannels++;
        }
        if (validChannels == 0) continue;

        StorylandAnimClip clip;
        clip.index = uint32_t(clipRows.size());
        clip.entryOffset = entryOffset;
        clip.channelTableOffset = channelTableOffset;
        clip.channelCount = channelCount;
        clip.flags = flags;
        clip.duration = clipDuration;
        clip.name = name;
        clipRows.push_back(clip);

        StorylandAnimField field;
        field.group = "Animation clip table";
        field.name = name;
        field.offset = entryOffset;
        field.value = channelTableOffset;
        std::ostringstream note;
        note << "channels=" << channelCount
             << " flags=" << animHex32(flags)
             << " duration=" << clipDuration
             << " table=" << animHexOffset(channelTableOffset);
        if (flags == 0xAAAAu) note << " / LCS PS2 retail clip-entry marker";
        field.note = note.str();
        fieldRows.push_back(field);
    }

    std::sort(clipRows.begin(), clipRows.end(), [](const StorylandAnimClip& a, const StorylandAnimClip& b) {
        return a.entryOffset < b.entryOffset;
    });
    for (uint32_t index = 0; index < clipRows.size(); ++index) clipRows[index].index = index;

    if (clipRows.empty()) return;

    activeClip = 0;
    rebuildActiveClipTracks();
}

void StorylandAnimFile::rebuildActiveClipTracks() {
    trackRows.clear();
    hasCandidates = false;

    if (clipRows.empty()) {
        frames = 1;
        duration = 1.0f / fps;
        return;
    }

    if (activeClip >= clipRows.size()) {
        activeClip = 0;
    }

    const StorylandAnimClip& clip = clipRows[activeClip];
    duration = clip.duration;

    struct RawDescriptor {
        uint32_t channelIndex = 0;
        uint32_t descriptorOffset = 0;
        uint32_t boneId = 0;
        uint32_t keyCount = 0;
        uint32_t keyOffset = 0;
        uint32_t tag = 0;
    };

    std::vector<RawDescriptor> descriptors;
    descriptors.reserve(clip.channelCount);
    for (uint32_t channelIndex = 0; channelIndex < clip.channelCount; ++channelIndex) {
        uint32_t descriptorOffset = clip.channelTableOffset + channelIndex * 12u;
        RawDescriptor descriptor;
        descriptor.channelIndex = channelIndex;
        descriptor.descriptorOffset = descriptorOffset;
        uint32_t channelType = animReadU16(data, descriptorOffset + 0);
        descriptor.keyCount = animReadU16(data, descriptorOffset + 2);
        descriptor.keyOffset = animReadU32(data, descriptorOffset + 4);
        descriptor.tag = animReadU32(data, descriptorOffset + 8);
        descriptor.boneId = animBoneIdFromChannelTag(descriptor.tag);

        // Known Leeds channel types observed so far:
        // 0x01 / 0x09 = 10-byte compressed rotation key stream.
        // 0x03 / 0x0B = 16-byte compressed rotation + translation key stream.
        if (!animChannelTypeIsKnown(channelType)) continue;

        if (descriptor.keyCount == 0 || descriptor.keyCount > 1000) continue;
        if (!animValidPointer(data, descriptor.keyOffset)) continue;
        descriptors.push_back(descriptor);
    }

    std::vector<uint32_t> keyOffsets;
    keyOffsets.reserve(descriptors.size() + 1);
    for (const auto& descriptor : descriptors) keyOffsets.push_back(descriptor.keyOffset);
    uint32_t dataEnd = animDataEndGuess(data);
    keyOffsets.push_back(dataEnd);
    std::sort(keyOffsets.begin(), keyOffsets.end());
    keyOffsets.erase(std::unique(keyOffsets.begin(), keyOffsets.end()), keyOffsets.end());

    auto nextKeyOffsetAfter = [&](uint32_t keyOffset) -> uint32_t {
        for (uint32_t candidate : keyOffsets) {
            if (candidate > keyOffset) return candidate;
        }
        return uint32_t(data.size());
    };

    for (const auto& descriptor : descriptors) {
        uint32_t nextOffset = nextKeyOffsetAfter(descriptor.keyOffset);
        if (nextOffset <= descriptor.keyOffset) continue;

        uint32_t available = std::min<uint32_t>(nextOffset - descriptor.keyOffset, uint32_t(data.size() - descriptor.keyOffset));
        uint32_t channelType = animReadU16(data, descriptor.descriptorOffset + 0);
        uint32_t stride = animStrideForChannelType(channelType);
        if (available < descriptor.keyCount * stride) continue;

        StorylandAnimTrack track;
        track.index = uint32_t(trackRows.size());
        track.clipIndex = activeClip;
        track.channelIndex = descriptor.channelIndex;
        track.offset = descriptor.descriptorOffset;
        track.keyDataOffset = descriptor.keyOffset;
        track.keyStride = stride;
        track.boneId = descriptor.boneId;
        uint32_t canonicalIndex = animCanonicalIndexForBoneId(descriptor.boneId);
        track.boneIndex = canonicalIndex != 0xFFFFFFFFu ? canonicalIndex : descriptor.channelIndex;
        track.parentIndex = animCanonicalParent(track.boneIndex);
        track.tag = descriptor.tag;
        track.name = animNameForBoneId(descriptor.boneId, track.boneIndex);

        std::ostringstream source;
        uint32_t sourceChannelType = animReadU16(data, descriptor.descriptorOffset + 0);
        source << "clip '" << clip.name << "' channel=" << descriptor.channelIndex
               << " flags=0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << sourceChannelType << std::dec
               << " boneId=" << descriptor.boneId
               << " keys=" << descriptor.keyCount
               << " stride=" << stride
               << " desc=" << animHexOffset(descriptor.descriptorOffset)
               << " keyData=" << animHexOffset(descriptor.keyOffset)
               << " tag=" << animHex32(descriptor.tag);
        if ((sourceChannelType & 0x0010u) != 0) source << " LCS_PS2_extra10";
        if ((sourceChannelType & 0x0008u) != 0) source << " VCS_or_PSP_extra08";
        track.source = source.str();

        track.keys.reserve(descriptor.keyCount);
        float accumulatedTime = 0.0f;
        for (uint32_t keyIndex = 0; keyIndex < descriptor.keyCount; ++keyIndex) {
            uint32_t keyOffset = descriptor.keyOffset + keyIndex * stride;
            if (keyOffset + stride > data.size()) break;

            StorylandAnimKey key = decodeAnimKeyWithFlags(data, keyOffset, channelType);
            float deltaSeconds = key.time;
            if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) deltaSeconds = 0.0f;

            accumulatedTime += deltaSeconds;
            key.time = accumulatedTime;
            if (keyIndex == 0) key.time = 0.0f;

            track.keys.push_back(key);
        }

        if (track.keys.size() > 1 && clip.duration > 0.0f && accumulatedTime > 0.0001f) {
            if (accumulatedTime < clip.duration * 0.50f || accumulatedTime > clip.duration * 1.50f) {
                float scale = clip.duration / accumulatedTime;
                for (auto& key : track.keys) {
                    key.time *= scale;
                }
            }
        }

        if (!track.keys.empty()) trackRows.push_back(track);
    }

    if (!trackRows.empty()) hasCandidates = true;

    uint32_t maxKeys = 1;
    for (const auto& track : trackRows) {
        maxKeys = std::max<uint32_t>(maxKeys, uint32_t(track.keys.size()));
    }

    frames = std::max<uint32_t>(1, maxKeys);
    if (clip.duration > 0.0f) {
        duration = clip.duration;
    } else {
        duration = std::max(1.0f / fps, float(std::max<uint32_t>(1, frames - 1)) / fps);
    }
}

void StorylandAnimFile::buildStaticInspectionTracks() {
    const auto& names = animCanonicalBoneNames();
    uint32_t trackCount = uint32_t(names.size());
    frames = 1;
    duration = 1.0f / fps;

    for (uint32_t trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
        StorylandAnimTrack track;
        track.index = trackIndex;
        track.boneIndex = trackIndex;
        track.parentIndex = animCanonicalParent(trackIndex);
        track.boneId = animBoneIdForCanonicalIndex(trackIndex);
        track.name = names[trackIndex];
        track.offset = 0;
        track.keyDataOffset = 0;
        track.keyStride = 0;
        track.source = "static inspection track; no Leeds clip/channel descriptors decoded";

        StorylandAnimKey key;
        key.time = 0.0f;
        key.offset = 0;
        track.keys.push_back(key);
        trackRows.push_back(track);
    }
}

std::vector<StorylandAnimPoseBone> StorylandAnimFile::poseAt(float seconds) const {
    const auto& names = animCanonicalBoneNames();
    std::vector<StorylandAnimPoseBone> pose;
    pose.reserve(names.size());
    for (uint32_t index = 0; index < names.size(); ++index) {
        pose.push_back(animBasePoseBone(index));
    }

    if (duration > 0.0f) {
        seconds = std::fmod(std::max(0.0f, seconds), duration);
    }

    // Standalone view is only a skeleton probe. Full MDL application is done in main.cpp
    // against the current model's imported RslTAnim hierarchy, using bone ids where available.
    for (const auto& track : trackRows) {
        if (track.keys.empty()) continue;

        uint32_t targetIndex = track.boneIndex;
        for (size_t i = 0; i < pose.size(); ++i) {
            if (pose[i].boneId == track.boneId && track.boneId != 0xFFFFFFFFu) {
                targetIndex = uint32_t(i);
                break;
            }
        }

        if (targetIndex >= pose.size()) continue;

        const StorylandAnimKey* a = &track.keys.front();
        const StorylandAnimKey* b = &track.keys.back();

        for (size_t i = 0; i < track.keys.size(); ++i) {
            if (track.keys[i].time <= seconds) a = &track.keys[i];
            if (track.keys[i].time >= seconds) {
                b = &track.keys[i];
                break;
            }
        }

        float span = std::max(0.0001f, b->time - a->time);
        float factor = std::max(0.0f, std::min(1.0f, (seconds - a->time) / span));
        float tx = a->tx * (1.0f - factor) + b->tx * factor;
        float ty = a->ty * (1.0f - factor) + b->ty * factor;
        float tz = a->tz * (1.0f - factor) + b->tz * factor;

        if (track.keyStride == 16) {
            pose[targetIndex].x += std::max(-0.20f, std::min(0.20f, tx * 0.20f));
            pose[targetIndex].y += std::max(-0.20f, std::min(0.20f, ty * 0.20f));
            pose[targetIndex].z += std::max(-0.20f, std::min(0.20f, tz * 0.20f));
        }
    }

    return pose;
}

const std::wstring& StorylandAnimFile::sourcePath() const { return path; }
const std::vector<uint8_t>& StorylandAnimFile::rawBytes() const { return data; }
const std::vector<StorylandAnimField>& StorylandAnimFile::fields() const { return fieldRows; }
const std::vector<StorylandAnimTrack>& StorylandAnimFile::tracks() const { return trackRows; }
const std::vector<StorylandAnimClip>& StorylandAnimFile::clips() const { return clipRows; }
const std::vector<std::string>& StorylandAnimFile::stringHints() const { return strings; }
float StorylandAnimFile::durationSeconds() const { return duration; }
float StorylandAnimFile::framesPerSecond() const { return fps; }
uint32_t StorylandAnimFile::frameCount() const { return frames; }
uint32_t StorylandAnimFile::activeClipIndex() const { return activeClip; }

bool StorylandAnimFile::setActiveClipIndex(uint32_t clipIndex) {
    if (clipIndex >= clipRows.size()) {
        return false;
    }

    if (activeClip == clipIndex && !trackRows.empty()) {
        return true;
    }

    activeClip = clipIndex;
    rebuildActiveClipTracks();

    if (trackRows.empty()) {
        trackRows.clear();
        buildStaticInspectionTracks();
    }

    return true;
}
bool StorylandAnimFile::hasKeyframeCandidates() const { return hasCandidates; }
bool StorylandAnimFile::hasDecodedMotion() const { return hasCandidates && !clipRows.empty(); }

std::string StorylandAnimFile::summaryLine() const {
    std::ostringstream ss;
    ss << ".anim size=" << data.size()
       << " bytes clips=" << clipRows.size()
       << " tracks=" << trackRows.size()
       << " frames=" << frames
       << " duration=" << std::fixed << std::setprecision(2) << duration << "s";
    if (!clipRows.empty()) ss << " activeClip='" << clipRows[activeClip].name << "'";
    if (hasCandidates) {
        bool hasLcs10 = false;
        bool hasVcs08 = false;
        for (const auto& track : trackRows) {
            if (track.source.find("LCS_PS2_extra10") != std::string::npos) hasLcs10 = true;
            if (track.source.find("VCS_or_PSP_extra08") != std::string::npos) hasVcs08 = true;
        }
        ss << " Leeds compressed transform channels=yes";
        if (hasLcs10) ss << " format=LCS_PS2_0x10";
        else if (hasVcs08) ss << " format=VCS_or_PSP_0x08";
    } else {
        ss << " transform channels=no, static inspection only";
    }
    return ss.str();
}
