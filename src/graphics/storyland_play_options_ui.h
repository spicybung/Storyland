#pragma once

#include "imgui.h"
#include <algorithm>

class StorylandPlayOptionsSection {
public:
    StorylandPlayOptionsSection(const char* label, bool menu, bool defaultOpen = false)
        : menu(menu), opened(menu ? ImGui::BeginMenu(label) :
            ImGui::CollapsingHeader(label, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        if(!opened)return;
        ImGui::PushID(label);
        const float scale=ImGui::GetFontSize()/13.0f;
        const float width=menu?360.0f*scale:std::max(1.0f,ImGui::GetContentRegionAvail().x);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width);
        ImGui::PushItemWidth(std::min(220.0f*scale,width*.6f));
        if(!menu)ImGui::Indent();
    }

    ~StorylandPlayOptionsSection() {
        if(!opened)return;
        if(!menu)ImGui::Unindent();
        ImGui::PopItemWidth();
        ImGui::PopTextWrapPos();
        ImGui::PopID();
        if(menu)ImGui::EndMenu();
    }

    explicit operator bool() const {return opened;}
    StorylandPlayOptionsSection(const StorylandPlayOptionsSection&)=delete;
    StorylandPlayOptionsSection& operator=(const StorylandPlayOptionsSection&)=delete;

private:
    bool menu=false;
    bool opened=false;
};

inline bool storylandPlayOptionToggle(const char* label, bool* value, bool menu, bool enabled = true) {
    if(menu)return ImGui::MenuItem(label,nullptr,value,enabled);
    ImGui::BeginDisabled(!enabled);
    const bool changed=ImGui::Checkbox(label,value);
    ImGui::EndDisabled();
    return changed;
}

inline void storylandPlayOptionTooltip(const char* text) {
    const float width=std::max(1.0f,std::min(420.0f*ImGui::GetFontSize()/13.0f,
        ImGui::GetIO().DisplaySize.x-32.0f));
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}
