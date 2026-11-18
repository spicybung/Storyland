#pragma once
#include "src/leeds/leeds_model.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <string_view>

inline std::string storylandMaterialTextureName(const StorylandModelFile& model, size_t materialIndex) {
    const auto& names = model.previewMaterialTextureNames();
    if (materialIndex >= names.size()) return {};
    if (!names[materialIndex].empty()) return names[materialIndex];

    const auto& bytes = model.rawBytes();
    if (model.modelKind() != StorylandModelKind::PedModel || names.size() != 2 ||
        bytes.size() < 4 || bytes[0] != 'l' || bytes[1] != 'd' || bytes[2] != 'm' || bytes[3] != 0)
        return {};

    std::wstring_view filename(model.sourcePath());
    const size_t separator = filename.find_last_of(L"/\\");
    if (separator != std::wstring_view::npos) filename.remove_prefix(separator + 1);
    constexpr std::wstring_view playerFilename = L"player.mdl";
    if (filename.size() != playerFilename.size() ||
        !std::equal(filename.begin(), filename.end(), playerFilename.begin(),
            [](wchar_t a, wchar_t b) { return std::towlower(a) == b; })) return {};

    const auto& bodyName = names[materialIndex == 0 ? 1 : 0];
    constexpr std::string_view bodyTexture = "ig_mafia_body";
    if (bodyName.size() != bodyTexture.size() ||
        !std::equal(bodyName.begin(), bodyName.end(), bodyTexture.begin(),
            [](unsigned char a, char b) { return std::tolower(a) == b; })) return {};

    // Retail LCS player.mdl leaves its skin material unnamed. The matching
    // player.chk contains this head raster alongside IG_mafia_body.
    return "toni_head128_X_8bit";
}
