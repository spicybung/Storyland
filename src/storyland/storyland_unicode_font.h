#pragma once

#include "imgui.h"
#include "src/storyland/storyland_cyrillic_font_data.inl"

inline const ImWchar* storylandUnicodeGlyphRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x017F,
        0x0400, 0x052F,
        0x2010, 0x2027,
        0x20AC, 0x20AC,
        0x2DE0, 0x2DFF,
        0xA640, 0xA69F,
        0,
    };
    return ranges;
}

inline ImFont* storylandAddUnicodeFont(float pixels = 13.0f) {
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    config.OversampleH = 2;
    config.OversampleV = 2;
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(storylandCyrillicFontData),
        int(sizeof(storylandCyrillicFontData)), pixels, &config,
        storylandUnicodeGlyphRanges());
}

inline void storylandInitializeUnicodeFont() {
    auto& io = ImGui::GetIO();
    io.FontDefault = storylandAddUnicodeFont();
    if (!io.FontDefault) io.FontDefault = io.Fonts->AddFontDefault();
}
