#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct StorylandNativeMaterialEnvironment {
    std::string texture;
    float intensity = 0.0f;
};

inline uint32_t storylandNativeMaterialWord(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) return 0;
    uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

inline std::string storylandNativeMaterialRasterName(const std::vector<uint8_t>& bytes, uint32_t offset) {
    if (offset == 0 || offset >= bytes.size()) return {};
    std::string name;
    for (size_t index = 0; index < 32 && size_t(offset) + index < bytes.size(); ++index) {
        const uint8_t character = bytes[size_t(offset) + index];
        if (character == 0) break;
        const bool valid = (character >= '0' && character <= '9') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') ||
            character == '_' || character == '-' || character == '.';
        if (!valid) return {};
        name.push_back(char(character));
    }
    if (name.empty() || name.size() == 32) return {};
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) {
        return char(character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character);
    });
    return name;
}

inline StorylandNativeMaterialEnvironment storylandReadNativeMaterialEnvironment(
    const std::vector<uint8_t>& bytes, uint32_t material, bool residentTexturePointers = false) {
    StorylandNativeMaterialEnvironment result;
    if (material == 0 || material == UINT32_MAX ||
        uint64_t(material) + 16 > bytes.size()) return result;

    // RslMaterial +0x0C contains the native RslMatFX pointer.
    const uint32_t effect = storylandNativeMaterialWord(bytes, size_t(material) + 12);
    if (effect == 0 || uint64_t(effect) + 16 > bytes.size()) return result;
    // The native LCS and VCS vehicle passes select reflection flag 2.
    // Combined flags (including retail LCS value 3) use the same coefficient.
    if ((storylandNativeMaterialWord(bytes, size_t(effect) + 12) & 2u) == 0) return result;

    const uint32_t bits = storylandNativeMaterialWord(bytes, size_t(effect) + 8);
    float intensity = 0.0f;
    std::memcpy(&intensity, &bits, sizeof(intensity));
    if (!std::isfinite(intensity) || intensity <= 0.0f) return result;
    result.intensity = intensity;

    // Before loading the archive, +0x04 points to a name string. After
    // relocation it can instead point to an RslTexture (name at +0x10).
    const uint32_t texture = storylandNativeMaterialWord(bytes, size_t(effect) + 4);
    if (residentTexturePointers && uint64_t(texture) + 0x10 < bytes.size())
        result.texture = storylandNativeMaterialRasterName(bytes, texture + 0x10);
    if (result.texture.empty())
        result.texture = storylandNativeMaterialRasterName(bytes, texture);
    if (result.texture.empty() && uint64_t(texture) + 0x10 < bytes.size())
        result.texture = storylandNativeMaterialRasterName(bytes, texture + 0x10);
    return result;
}
