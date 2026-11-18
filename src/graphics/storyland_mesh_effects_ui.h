#pragma once

#include "src/graphics/storyland_mesh_effects.h"
#include "imgui.h"

inline void storylandMeshEffectMenu(StorylandMeshEffects& effects,float time) {
    // Each category gets one effect. The Everything item just fills the same slots.
    constexpr const char* names[]={"Bouncy","Jelly","Melt","Curl-Noise Deformation","Harmonic Cage Deformation"};
    constexpr const char* categories[]={"Cars","Characters","World","Weapons"};
    for(size_t index=0;index<std::size(names);++index) {
        const auto mode=StorylandMeshEffect(index+1);
        if(!ImGui::BeginMenu(names[index]))continue;
        bool all=std::all_of(effects.categories.begin(),effects.categories.end(),
            [&](const auto& value){return value.mode==mode;});
        if(ImGui::MenuItem("Everything",nullptr,&all)) {
            for(auto& setting:effects.categories) {
                if(all)setting={mode,time};
                else if(setting.mode==mode)setting={};
            }
        }
        for(size_t category=0;category<effects.categories.size();++category) {
            auto& setting=effects.categories[category];
            bool enabled=setting.mode==mode;
            if(ImGui::MenuItem(categories[category],nullptr,&enabled))
                setting={enabled?mode:StorylandMeshEffect::Off,time};
        }
        ImGui::SliderFloat("Strength",&effects.strength,0,1,"%.2f");
        ImGui::EndMenu();
    }
}
