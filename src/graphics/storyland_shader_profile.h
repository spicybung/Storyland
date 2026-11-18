#pragma once

#include <array>
#include <cstddef>

inline constexpr int StorylandPcShaderProfile = 16;
inline constexpr unsigned StorylandPcShaderCommand = 50191;

inline constexpr bool storylandPcShaderProfile(int profile) {
    return profile == StorylandPcShaderProfile;
}

inline constexpr int storylandShaderRenderProfile(int profile) {
    return storylandPcShaderProfile(profile) ? 2 : profile;
}

inline constexpr bool storylandSelectableShaderProfile(int profile) {
    return profile >= -1 && profile <= StorylandPcShaderProfile &&
        profile != 8 && profile != 13;
}

inline constexpr std::array<int, 16> StorylandPortableShaderProfiles{{
    9, -1, 0, 1, 2, StorylandPcShaderProfile, 3, 4, 14, 15, 5, 12, 11, 6, 7, 10
}};

inline constexpr std::array<const char*, 16> StorylandPortableShaderNames{{
    "San Andreas", "Default", "LCS PS2", "LCS PSP", "VCS PS2", "PC", "VCS PSP",
    "Neo (SkyGFX)", "Cel", "Budokai 1", "CTW PSP", "Aero Low", "Aero Medium (Phong)",
    "Aero High (Blinn-Phong)", "Aero Ultra High (Blinn-Phong)", "Mafia 2"
}};

inline int storylandShaderSelectionIndex(int profile) {
    for (std::size_t index = 0; index < StorylandPortableShaderProfiles.size(); ++index)
        if (StorylandPortableShaderProfiles[index] == profile) return int(index);
    return 1;
}
