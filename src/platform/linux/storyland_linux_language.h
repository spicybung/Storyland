#pragma once

#include "src/storyland/storyland_language.h"

#include <codecvt>
#include <filesystem>
#include <fstream>
#include <locale>
#include <string>
#include <system_error>

inline std::string storylandLinuxUiText(const wchar_t* english) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(storylandUiText(english));
}

inline void storylandLoadLinuxUiLanguage(const std::filesystem::path& path) {
    std::ifstream saved(path);
    unsigned selected = 0;
    if (saved >> selected && selected <= static_cast<unsigned>(StorylandUiLanguage::Russian))
        gStorylandUiLanguage = static_cast<StorylandUiLanguage>(selected);
}

inline bool storylandSaveLinuxUiLanguage(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream saved(path, std::ios::trunc);
    saved << static_cast<unsigned>(gStorylandUiLanguage) << '\n';
    saved.close();
    return bool(saved);
}
