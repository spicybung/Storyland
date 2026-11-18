#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>

struct StorylandAndroidLayout {
    ImVec2 origin, size;
    float unit = 1, toolbarHeight = 0, rowHeight = 0;
    bool splitResources = false;
    float resourceWidth = 0;
};

inline StorylandAndroidLayout storylandAndroidLayout(ImVec2 display, float density,
    float zoom, ImVec4 safe) {
    StorylandAndroidLayout layout;
    layout.unit = std::clamp((std::isfinite(density) ? density : 1.f) * .80f, .9f, 3.2f) * std::clamp(zoom, .8f, 1.3f);
    layout.origin = {std::max(0.f,safe.x), std::max(0.f,safe.y)};
    layout.size = {std::max(1.f,display.x-layout.origin.x-std::max(0.f,safe.z)),
                   std::max(1.f,display.y-layout.origin.y-std::max(0.f,safe.w))};
    layout.toolbarHeight = 49*layout.unit;
    layout.rowHeight = 38*layout.unit;
    layout.splitResources = layout.size.x/layout.unit >= 590 && layout.size.y/layout.unit >= 220;
    layout.resourceWidth = std::clamp(layout.size.x*.32f,175*layout.unit,280*layout.unit);
    return layout;
}

inline ImGuiStyle storylandAndroidStyle(float unit) {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.WindowPadding = {9,7};
    style.FramePadding = {9,6};
    style.ItemSpacing = {7,5};
    style.ItemInnerSpacing = {6,4};
    style.TouchExtraPadding = {2,2};
    style.ScrollbarSize = 12;
    style.GrabMinSize = 20;
    style.WindowRounding = 10;
    style.ChildRounding = 8;
    style.FrameRounding = 7;
    style.PopupRounding = 8;
    style.TabRounding = 6;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 1;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.075f,.065f,.085f,1);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(.105f,.09f,.115f,1);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(.17f,.14f,.19f,1);
    style.Colors[ImGuiCol_Button] = ImVec4(.42f,.10f,.27f,1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.62f,.16f,.39f,1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(.75f,.20f,.46f,1);
    style.Colors[ImGuiCol_Header] = ImVec4(.42f,.10f,.27f,1);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(.55f,.15f,.34f,1);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(.65f,.18f,.40f,1);
    style.Colors[ImGuiCol_TabSelected] = ImVec4(.42f,.10f,.27f,1);
    style.Colors[ImGuiCol_Tab] = ImVec4(.18f,.14f,.21f,1);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(.55f,.15f,.34f,1);
    style.Colors[ImGuiCol_Text] = ImVec4(.94f,.93f,.96f,1);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(.65f,.60f,.69f,1);
    style.ScaleAllSizes(unit);
    return style;
}

inline bool storylandTouchButton(const char* label, float unit, float width = 0) {
    return ImGui::Button(label,{width > 0 ? width*unit : 0,38*unit});
}
