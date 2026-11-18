#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cwctype>

inline uint32_t storylandRwVersion(uint32_t stamp) {
    return (stamp & 0xFFFF0000u)
        ? (((stamp >> 14u) & 0x3FF00u) + 0x30000u) | ((stamp >> 16u) & 0x3Fu)
        : stamp << 8u;
}

enum class StorylandRwGame { Unknown, GTA3, ViceCity, SanAndreas };

inline StorylandRwGame storylandRwPathGame(std::wstring path) {
    for (wchar_t& character : path) character = wchar_t(std::towlower(character));
    if (path.find(L"stories") != std::wstring::npos || path.find(L"lcs") != std::wstring::npos ||
        path.find(L"vcs") != std::wstring::npos) return StorylandRwGame::Unknown;
    if (path.find(L"gta3") != std::wstring::npos || path.find(L"gta iii") != std::wstring::npos ||
        path.find(L"grand theft auto iii") != std::wstring::npos) return StorylandRwGame::GTA3;
    if (path.find(L"vice city") != std::wstring::npos || path.find(L"gtavc") != std::wstring::npos)
        return StorylandRwGame::ViceCity;
    if (path.find(L"san andreas") != std::wstring::npos || path.find(L"gtasa") != std::wstring::npos ||
        path.find(L"gta sa") != std::wstring::npos) return StorylandRwGame::SanAndreas;
    return StorylandRwGame::Unknown;
}

// Compatibility family, not proof of which game authored a converted asset.
inline StorylandRwGame storylandRwGame(uint32_t stamp) {
    const uint32_t version = storylandRwVersion(stamp);
    if (version < 0x30000u || version >= 0x40000u) return StorylandRwGame::Unknown;
    if (version < 0x33000u) return StorylandRwGame::GTA3;
    if (version < 0x34000u) return StorylandRwGame::ViceCity;
    return StorylandRwGame::SanAndreas;
}

inline uint32_t storylandRwStamp(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 12u) return 0u;
    return uint32_t(bytes[8]) | (uint32_t(bytes[9]) << 8u) |
           (uint32_t(bytes[10]) << 16u) | (uint32_t(bytes[11]) << 24u);
}
