#pragma once

#include "src/graphics/storyland_graphics_quality.h"
#include "imgui.h"
#include <string>

inline bool storylandDrawGraphicsAdvancedOptions(StorylandGraphicsQualitySettings& settings,
    float maximumAnisotropy,bool targetProfile,const std::string& fxaaStatus={}) {
    bool changed=false;
    if(ImGui::BeginMenu("Advanced")) {
        changed|=ImGui::Checkbox("FXAA",&settings.fxaa);
        changed|=ImGui::Checkbox("Anisotropic Filtering 16x",&settings.anisotropic16x);
        if(!targetProfile)
            ImGui::TextWrapped("Applies to LCS PS2, VCS PS2, PC, Neo and Aero.");
        if(settings.anisotropic16x&&maximumAnisotropy<16.0f) {
            if(maximumAnisotropy<=1.0f)
                ImGui::TextWrapped("Anisotropic filtering is unavailable on this device.");
            else ImGui::TextWrapped("Limited to %.0fx on this device.",maximumAnisotropy);
        }
        if(settings.fxaa&&!fxaaStatus.empty())ImGui::TextWrapped("%s",fxaaStatus.c_str());
        ImGui::EndMenu();
    }
    return changed;
}
